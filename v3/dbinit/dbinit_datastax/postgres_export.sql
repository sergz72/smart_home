select sensor_id,
       to_char(to_timestamp((event_date::bigint * 1000000 + event_time)::varchar, 'YYYYMMDDHH24MISS'), 'YYYY-MM-DD HH24:MI:SS') datetime,
       json_object_agg(value_type, value) values
from sensor_events
group by 1,2
