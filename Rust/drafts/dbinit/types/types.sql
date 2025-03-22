CREATE TYPE device_sensor AS (
  id smallint,
  value_type char(4)
);
CREATE TYPE sensor_offset AS (
  value_type char(4),
  offset_value integer
);
