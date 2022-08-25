CREATE USER collector;
ALTER USER collector PASSWORD 'collector';

CREATE USER grafana;
ALTER USER grafana PASSWORD 'grafana';

CREATE DATABASE housemetrics;

\c housemetrics;

CREATE EXTENSION IF NOT EXISTS timescaledb;

CREATE TABLE airquality(
  time TIMESTAMP WITHOUT TIME ZONE NOT NULL,
  pm1 DOUBLE PRECISION,
  pm2 DOUBLE PRECISION,
  pm10 DOUBLE PRECISION,
  co2 DOUBLE PRECISION,
  temp DOUBLE PRECISION,
  rh DOUBLE PRECISION,
  id INTEGER NOT NULL
);

SELECT create_hypertable('airquality', 'time');

ALTER TABLE airquality SET (
  timescaledb.compress,
  timescaledb.compress_segmentby = 'id'
);

SELECT add_compression_policy(
  'airquality',
  INTERVAL '7 days'
);

CREATE TABLE sensors(
  id SERIAL PRIMARY KEY,
  name VARCHAR(32) NOT NULL
);

CREATE MATERIALIZED VIEW airquality_1m
WITH (timescaledb.continuous) AS
SELECT
  id,
  time_bucket(INTERVAL '1 minute', time) AS bucket,
  min(pm1) AS min_pm1,
  avg(pm1) AS avg_pm1,
  max(pm1) AS max_pm1,
  min(pm2) AS min_pm2,
  avg(pm2) AS avg_pm2,
  max(pm2) AS max_pm2,
  min(pm10) AS min_pm10,
  avg(pm10) AS avg_pm10,
  max(pm10) AS max_pm10,
  min(co2) AS min_co2,
  avg(co2) AS avg_co2,
  max(co2) AS max_co2,
  min(temp) AS min_temp,
  avg(temp) AS avg_temp,
  max(temp) AS max_temp,
  min(rh) AS min_rh,
  avg(rh) AS avg_rh,
  max(rh) AS max_rh,
  min(humidex) AS min_humidex,
  avg(humidex) AS avg_humidex,
  max(humidex) AS max_humidex
FROM airquality
GROUP BY id, bucket;

SELECT add_continuous_aggregate_policy(
  'airquality_1m',
  start_offset => INTERVAL '15 minutes',
  end_offset => INTERVAL '5 minutes',
  schedule_interval => INTERVAL '5 minutes'
);

ALTER MATERIALIZED VIEW airquality_1m SET(
  timescaledb.compress = true
);

SELECT add_compression_policy(
  'airquality_1m',
  compress_after => '1 hour'::interval
);

CREATE MATERIALIZED VIEW airquality_5m
WITH (timescaledb.continuous) AS
SELECT
  id,
  time_bucket(INTERVAL '5 minutes', time) AS bucket,
  min(pm1) AS min_pm1,
  avg(pm1) AS avg_pm1,
  max(pm1) AS max_pm1,
  min(pm2) AS min_pm2,
  avg(pm2) AS avg_pm2,
  max(pm2) AS max_pm2,
  min(pm10) AS min_pm10,
  avg(pm10) AS avg_pm10,
  max(pm10) AS max_pm10,
  min(co2) AS min_co2,
  avg(co2) AS avg_co2,
  max(co2) AS max_co2,
  min(temp) AS min_temp,
  avg(temp) AS avg_temp,
  max(temp) AS max_temp,
  min(rh) AS min_rh,
  avg(rh) AS avg_rh,
  max(rh) AS max_rh,
  min(humidex) AS min_humidex,
  avg(humidex) AS avg_humidex,
  max(humidex) AS max_humidex
FROM airquality
GROUP BY id, bucket;

SELECT add_continuous_aggregate_policy(
  'airquality_5m',
  start_offset => INTERVAL '25 minutes',
  end_offset => INTERVAL '10 minutes',
  schedule_interval => INTERVAL '5 minutes'
);

ALTER MATERIALIZED VIEW airquality_5m SET(
  timescaledb.compress = true
);

SELECT add_compression_policy(
  'airquality_5m',
  compress_after => '1 hour'::interval
);

GRANT SELECT ON airquality_5m TO grafana;

CREATE MATERIALIZED VIEW airquality_1h
WITH (timescaledb.continuous) AS
SELECT
  id,
  time_bucket(INTERVAL '1 hour', time) AS bucket,
  min(pm1) AS min_pm1,
  avg(pm1) AS avg_pm1,
  max(pm1) AS max_pm1,
  min(pm2) AS min_pm2,
  avg(pm2) AS avg_pm2,
  max(pm2) AS max_pm2,
  min(pm10) AS min_pm10,
  avg(pm10) AS avg_pm10,
  max(pm10) AS max_pm10,
  min(co2) AS min_co2,
  avg(co2) AS avg_co2,
  max(co2) AS max_co2,
  min(temp) AS min_temp,
  avg(temp) AS avg_temp,
  max(temp) AS max_temp,
  min(rh) AS min_rh,
  avg(rh) AS avg_rh,
  max(rh) AS max_rh,
  min(humidex) AS min_humidex,
  avg(humidex) AS avg_humidex,
  max(humidex) AS max_humidex
FROM airquality
GROUP BY id, bucket;

SELECT add_continuous_aggregate_policy(
  'airquality_1h',
  start_offset => INTERVAL '3 hours',
  end_offset => INTERVAL '1 hour',
  schedule_interval => INTERVAL '15 minutes'
);

ALTER MATERIALIZED VIEW airquality_1h SET(
  timescaledb.compress = true
);

SELECT add_compression_policy(
  'airquality_1h',
  compress_after => '1 day'::interval
);

GRANT SELECT ON airquality_1h TO grafana;

CREATE MATERIALIZED VIEW airquality_1d
WITH (timescaledb.continuous) AS
SELECT
  id,
  time_bucket(INTERVAL '1 day', time) AS bucket,
  min(pm1) AS min_pm1,
  avg(pm1) AS avg_pm1,
  max(pm1) AS max_pm1,
  min(pm2) AS min_pm2,
  avg(pm2) AS avg_pm2,
  max(pm2) AS max_pm2,
  min(pm10) AS min_pm10,
  avg(pm10) AS avg_pm10,
  max(pm10) AS max_pm10,
  min(co2) AS min_co2,
  avg(co2) AS avg_co2,
  max(co2) AS max_co2,
  min(temp) AS min_temp,
  avg(temp) AS avg_temp,
  max(temp) AS max_temp,
  min(rh) AS min_rh,
  avg(rh) AS avg_rh,
  max(rh) AS max_rh,
  min(humidex) AS min_humidex,
  avg(humidex) AS avg_humidex,
  max(humidex) AS max_humidex
FROM airquality
GROUP BY id, bucket;

SELECT add_continuous_aggregate_policy(
  'airquality_1d',
  start_offset => INTERVAL '4 days',
  end_offset => INTERVAL '25 hours',
  schedule_interval => INTERVAL '12 hours'
);

ALTER MATERIALIZED VIEW airquality_1d SET(
  timescaledb.compress = true
);

SELECT add_compression_policy(
  'airquality_1d',
  compress_after => '5 days'::interval
);

GRANT SELECT ON airquality_1d TO grafana;

CREATE TABLE circuits(
  id SERIAL PRIMARY KEY,
  virtual BOOLEAN NOT NULL,
  name VARCHAR(128) NOT NULL,
  nominal_voltage DOUBLE PRECISION NOT NULL,
  max_amps DOUBLE PRECISION NOT NULL
);

CREATE TABLE electricity_tou_rate_types(
  id SERIAL PRIMARY KEY,
  year SMALLINT NOT NULL,
  start_month SMALLINT NOT NULL,
  name VARCHAR(64) NOT NULL,
  price DOUBLE PRECISION NOT NULL
);

CREATE TABLE electricity_tou_weekday_rates(
  year SMALLINT NOT NULL,
  start_month SMALLINT NOT NULL,
  start_hour SMALLINT NOT NULL,
  end_hour SMALLINT NOT NULL,
  type SMALLINT NOT NULL
);

CREATE TABLE electricity_tou_weekend_rate(
  year SMALLINT NOT NULL,
  start_month SMALLINT NOT NULL,
  type SMALLINT NOT NULL
);

CREATE TABLE electricity_tou_holidays(
  year SMALLINT NOT NULL,
  month SMALLINT NOT NULL,
  day SMALLINT NOT NULL,
  type SMALLINT NOT NULL
);

GRANT SELECT ON electricity_tou_types TO grafana;
GRANT SELECT ON electricity_tou_rates TO grafana, collector;
GRANT SELECT ON electricity_tou_holidays TO grafana, collector;

CREATE TABLE electricity_5s(
  time TIMESTAMP WITH TIME ZONE NOT NULL,
  circuit_id SMALLINT NOT NULL,
  watt_hours DOUBLE PRECISION NOT NULL,
  tou1_cost DOUBLE PRECISION,
  tou2_cost DOUBLE PRECISION,
  tou3_cost DOUBLE PRECISION
);

CREATE UNIQUE INDEX ON electricity_5s(circuit_id, time DESC);

SELECT create_hypertable('electricity_5s', 'time');

ALTER TABLE electricity_5s SET(
  timescaledb.compress,
  timescaledb.compress_segmentby = 'circuit_id'
);

SELECT add_compression_policy(
  'electricity_5s',
  INTERVAL '15 minute'
);

GRANT INSERT ON electricity_5s TO collector;
GRANT SELECT ON electricity_5s TO grafana;

CREATE MATERIALIZED VIEW electricity_1m
WITH (timescaledb.continuous) AS
SELECT
  time_bucket(INTERVAL '1 minute', time) AS bucket,
  circuit_id,
  SUM(watt_hours) AS watt_hours,
  MIN(watt_hours)*720 AS min_watts,
  MAX(watt_hours)*720 AS max_watts,
  SUM(COALESCE(tou1_cost, 0)) AS tou1_cost, 
  SUM(COALESCE(tou2_cost, 0)) AS tou2_cost, 
  SUM(COALESCE(tou3_cost, 0)) AS tou3_cost
FROM electricity_5s e
GROUP BY circuit_id, bucket;

SELECT add_continuous_aggregate_policy(
  'electricity_1m',
  start_offset => INTERVAL '15 minutes',
  end_offset => INTERVAL '5 minutes',
  schedule_interval => INTERVAL '5 minutes'
);

ALTER MATERIALIZED VIEW electricity_1m SET(
  timescaledb.compress = true
);

SELECT add_compression_policy(
  'electricity_1m',
  compress_after => '1 hour'::interval
);

GRANT SELECT ON electricity_1m TO grafana;

CREATE MATERIALIZED VIEW electricity_1h
WITH (timescaledb.continuous) AS
SELECT
  time_bucket(INTERVAL '1 hour', time) AS bucket,
  circuit_id,
  SUM(watt_hours) AS watt_hours,
  MIN(watt_hours)*720 AS min_watts,
  MAX(watt_hours)*720 AS max_watts,
  SUM(COALESCE(tou1_cost, 0)) AS tou1_cost, 
  SUM(COALESCE(tou2_cost, 0)) AS tou2_cost, 
  SUM(COALESCE(tou3_cost, 0)) AS tou3_cost
FROM electricity_5s e
GROUP BY circuit_id, bucket;

SELECT add_continuous_aggregate_policy(
  'electricity_1h',
  start_offset => INTERVAL '181 minutes',
  end_offset => INTERVAL '61 minutes',
  schedule_interval => INTERVAL '15 minutes'
);

ALTER MATERIALIZED VIEW electricity_1h SET(
  timescaledb.compress = true
);
SELECT add_continuous_aggregate_policy(
  'electricity_1h',
  start_offset => INTERVAL '181 minutes',
  end_offset => INTERVAL '61 minutes',
  schedule_interval => INTERVAL '15 minutes'
);

ALTER MATERIALIZED VIEW electricity_1h SET(
  timescaledb.compress = true
);

SELECT add_compression_policy(
  'electricity_1h',
  compress_after => '24 hour'::interval
);

GRANT SELECT ON electricity_1h TO grafana;

CREATE MATERIALIZED VIEW electricity_1d
WITH (timescaledb.continuous) AS
SELECT
  circuit_id,
  timescaledb_experimental.time_bucket_ng('1 day', time, timezone=>'America/Toronto') AS bucket, 
  SUM(watt_hours) AS watt_hours,
  MIN(watt_hours)*720 AS min_watts,
  MAX(watt_hours)*720 AS max_watts,
  SUM(COALESCE(tou1_cost, 0)) AS tou1_cost, 
  SUM(COALESCE(tou2_cost, 0)) AS tou2_cost, 
  SUM(COALESCE(tou3_cost, 0)) AS tou3_cost
FROM electricity_5s e
GROUP BY circuit_id, bucket;

SELECT add_continuous_aggregate_policy(
  'electricity_1d',
  start_offset => INTERVAL '73 hours',
  end_offset => INTERVAL '25 hour',
  schedule_interval => INTERVAL '1 hour'
);

ALTER MATERIALIZED VIEW electricity_1d SET(
  timescaledb.compress = true
);

SELECT add_compression_policy(
  'electricity_1d',
  compress_after => '7 day'::interval
);

GRANT SELECT ON electricity_1d TO grafana;
