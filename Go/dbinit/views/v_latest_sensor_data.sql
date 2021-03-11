CREATE OR REPLACE VIEW v_latest_sensor_data AS
WITH ranked AS (
  SELECT location_name, location_type, event_date, event_time, data_type, data, RANK() OVER (PARTITION BY location_name, data_type ORDER BY event_date DESC, event_time DESC) AS r
	FROM v_sensor_data
   where AGE(now(), event_date + event_time) <= INTERVAL '1 hour'
)
SELECT location_name, location_type, event_date, event_time, data_type, data
  FROM ranked
 WHERE r = 1
