using Microsoft.Extensions.Caching.Memory;
using Npgsql;
using System.Collections.Concurrent;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddMemoryCache();

var app = builder.Build();

var localTz = TimeZoneInfo.FindSystemTimeZoneById("America/Toronto");

NpgsqlConnection GetConnection() => new NpgsqlConnection(
	"Host=localhost;Username=collector;Password=redacted;Database=housemetrics"
);

double CalculateDewPoint(double t, double rh) {
        const double b = 17.368;
        const double c = 238.88;
        const double d = 234.5;

        var gamma = Math.Log(rh*Math.Exp((b - t/d)*(t/(c+t))));

        return c*gamma/(b - gamma);
}

double CalculateHumidex(double t, double rh) {
        const double ftoc = 5.0/9.0;

        var tdew = CalculateDewPoint(t, rh);

        var x = 5417.7530*(1/273.16-1/(273.15+tdew));

        return t + ftoc*(6.11*Math.Exp(x)-10.0);
}

async Task<int?> GetSensorIdAsync(NpgsqlConnection db, IMemoryCache cache, string name) {
	if (!cache.TryGetValue<int?>(name, out var id)) {
		await using var cmd = new NpgsqlCommand(
			@"SELECT id FROM sensors WHERE name = @name;",
			db
		);

		cmd.Parameters.AddWithValue("name", name);

		var result = await cmd.ExecuteScalarAsync();

		if (result == null) {
			cache.Set<int?>(
				key: name,
				value: null,
				options: new() { AbsoluteExpirationRelativeToNow = TimeSpan.FromSeconds(30) }
			);
		} else {	
			id = (int)result;

			cache.Set(
				key: name,
				value: id,
				options: new() { SlidingExpiration = TimeSpan.FromMinutes(1) }
			);
		}
	}

	return id;
}

// todo: merge this into /api/v2/write and switch on the first token
app.MapPut("/metrics/airquality", async (HttpRequest req, IMemoryCache cache) => {
	// example airquality sensor=kitchen temp=27.6 rh=47.6 co2=1234 tvoc=17 eco2=412 pm1=2.3 pm2=3.4 pm10=4.6
	using var body = new StreamReader(req.Body);

	using var db = GetConnection();

	await db.OpenAsync();

	while( true ) {
		var line = await body.ReadLineAsync();

		if( line == null ) {
			return Results.NoContent();
		}

		Console.WriteLine("got: " + line);

		var parts = line.Split(' ');
		var data = parts.Skip(1).Select(p => p.Split('='))
			.ToDictionary(p => p[0], p => p[1]);

		var sensorId = data["sensor"];

		var id = await GetSensorIdAsync(db, cache, sensorId);

		if (id == null) {
			return Results.NotFound();
		}

		await using var cmd = new NpgsqlCommand(
			@"INSERT INTO airquality(time, id, pm1, pm2, pm10, co2, temp, rh, humidex, tvoc, eco2)
			VALUES (CURRENT_TIMESTAMP, @id, @pm1, @pm2, @pm10, @co2, @temp, @rh, @humidex, @tvoc, @eco2)",
			db
		);

		cmd.Parameters.AddWithValue("id", id.Value);

		if( data.TryGetValue("co2", out var co2S ) && double.TryParse(co2S, out var co2) && co2 >= 350 ) {
			cmd.Parameters.AddWithNullableValue("co2", co2);
		}

		if( data.TryGetValue("pm1", out var pm1S ) && double.TryParse(pm1S, out var pm1 ) ) {
			cmd.Parameters.AddWithNullableValue("pm1", pm1);
		}
		if( data.TryGetValue("pm2", out var pm2S ) && double.TryParse(pm2S, out var pm2 ) ) {
			cmd.Parameters.AddWithNullableValue("pm2", pm2);
		}
		if( data.TryGetValue("pm10", out var pm10S ) && double.TryParse(pm10S, out var pm10 ) ) {
			cmd.Parameters.AddWithNullableValue("pm10", pm10);
		}
		if( data.TryGetValue("tvoc", out var tvocS ) && double.TryParse(tvocS, out var tvoc ) ) {
			cmd.Parameters.AddWithNullableValue("tvoc", tvoc);
		}
		if( data.TryGetValue("eco2", out var eco2S ) && double.TryParse(eco2S, out var eco2 ) ) {
			cmd.Parameters.AddWithNullableValue("eco2", eco2);
		}

		double temp = 0, rh = 0;
		var haveTemp = data.TryGetValue("temp", out var tempS) && double.TryParse(tempS, out temp);
		var haveRh = data.TryGetValue("rh", out var rhS) && double.TryParse(rhS, out rh);
		if( haveTemp ) {
			cmd.Parameters.AddWithNullableValue("temp", temp);
		}
		if( haveRh ) {
			cmd.Parameters.AddWithNullableValue("rh", rh);
		}
		if( haveTemp && haveRh ) {
			cmd.Parameters.AddWithNullableValue("humidex", CalculateHumidex(temp, rh/100.0));
		}

		await cmd.ExecuteNonQueryAsync();
	}
});

// dummy thing to make iotawatt happy
app.MapPost("/api/v2/query", () => {});

app.MapPost("/api/v2/write", async (HttpRequest req) => {
	// 9 v=0.0000000 1654732890
	using var body = new StreamReader(req.Body);

	await using var db = new NpgsqlConnection(
		"Host=localhost;Username=collector;Password=redacted;Database=housemetrics"
	);

	await db.OpenAsync();

	while( true ) {
		var line = await body.ReadLineAsync();

		if( line == null ) {
			return Results.NoContent();
		}

		var parts = line.Split(' ');

		var time = DateTimeOffset.FromUnixTimeSeconds( long.Parse(parts[2]));
		var wh = double.Parse(parts[1].Substring(2));

		// discard datapoints that are less than 1 milliwatt
		if( wh < 0.0000001) {
			continue;
		}

		if((DateTimeOffset.Now - time).TotalDays > 1) {
			continue;
		}

		await using var cmd = new NpgsqlCommand(
			@"INSERT INTO electricity_5s(time, circuit_id, watt_hours, tou1_cost, tou2_cost, tou3_cost)
			VALUES (@time, @id, @wh, @tou1_cost, @tou2_cost, @tou3_cost)",
			db
		);

		var id = int.Parse(parts[0]);
		cmd.Parameters.AddWithValue("time", time);
		cmd.Parameters.AddWithValue("id", id);
		cmd.Parameters.AddWithValue("wh", wh);

		var localTime = TimeZoneInfo.ConvertTimeFromUtc(time.UtcDateTime, localTz);
		
		if(localTime.DayOfWeek == DayOfWeek.Saturday || localTime.DayOfWeek == DayOfWeek.Sunday
		|| localTime.Hour >= 19 || localTime.Hour < 7
		|| (localTime.Month == 7 && localTime.Day == 1)
		|| (localTime.Month == 8 && localTime.Day == 1)
		|| (localTime.Month == 9 && localTime.Day == 5)
		|| (localTime.Month == 10 && localTime.Day == 10)
		|| (localTime.Month == 12 && localTime.Day == 26)
		|| (localTime.Month == 12 && localTime.Day == 27)) {
			cmd.Parameters.AddWithNullableValue("tou1_cost", wh*0.082/1000);
			cmd.Parameters.AddWithNullableValue("tou2_cost", null);
			cmd.Parameters.AddWithNullableValue("tou3_cost", null);
		} else if(localTime.Hour >= 11 && localTime.Hour < 17) {
			cmd.Parameters.AddWithNullableValue("tou1_cost", null);
			cmd.Parameters.AddWithNullableValue("tou2_cost", null);
			cmd.Parameters.AddWithNullableValue("tou3_cost", wh*0.17/1000);
		} else {
			cmd.Parameters.AddWithNullableValue("tou1_cost", null);
			cmd.Parameters.AddWithNullableValue("tou2_cost", wh*0.113/1000);
			cmd.Parameters.AddWithNullableValue("tou3_cost", null);
		}	
		try {
			await cmd.ExecuteNonQueryAsync();
		} catch( PostgresException e ) when (e.SqlState == "23505") {
			Console.WriteLine($"Discarding duplicate for {id} at {time}");
		}
	}
});

app.MapGet("/metrics/ping", () => "pong");

app.Run();
