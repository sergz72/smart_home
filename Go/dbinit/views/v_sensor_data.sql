create or replace view v_sensor_data as
select c.name location_name, c.location_type, a.event_date, a.event_time, b.data_type, a.data
from sensor_data a, sensors b, locations c
where a.sensor_id = b.id and b.location_id = c.id and a.error_message is null;