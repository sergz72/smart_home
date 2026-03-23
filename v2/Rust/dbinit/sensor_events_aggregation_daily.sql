delete from sensor_events_aggregated where event_date >= to_char(now() - interval '2 days', 'YYYYMMDD')::int;
insert into sensor_events_aggregated(event_date, sensor_id, value_type, min_value, max_value, avg_value)
select event_date, sensor_id, value_type, min(value) min_value, max(value) max_value, avg(value) avg_value
from sensor_events
where event_date >= to_char(now() - interval '2 days', 'YYYYMMDD')::int
group by event_date, sensor_id, value_type;
