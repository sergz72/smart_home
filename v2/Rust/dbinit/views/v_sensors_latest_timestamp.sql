CREATE VIEW v_sensors_latest_timestamp AS
select s.name, s.id, coalesce(max(event_date::BIGINT * 1000000 + event_time), 0) max_datetime
  from sensors s left join sensor_events e
    on e.sensor_id = s.id
   and e.event_date >= to_char(now() - interval '1 week', 'YYYYMMDD')::INT
   and event_date <= to_char(now() + interval '1 day', 'YYYYMMDD')::INT
group by s.name, s.id;
