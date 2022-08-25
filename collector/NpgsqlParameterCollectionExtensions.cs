using Npgsql;

internal static class NpgsqlParameterCollectionExtensions {
	public static void AddWithNullableValue(
		this NpgsqlParameterCollection @this,
		string name,
		object? val
	) => @this.AddWithValue(name, val ?? DBNull.Value); 
}
