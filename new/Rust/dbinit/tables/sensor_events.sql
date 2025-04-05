create table sensor_events (
  id integer not null default NEXTVAL('sensor_events_id_seq'),
  sensor_id smallint not null references sensors(id),
  event_date integer not null,
  event_time integer not null,
  value_type char(4) not null,
  value integer not null,
  constraint sensor_events_pk primary key(id, event_date),
  constraint sensor_events_uk unique(sensor_id, event_date, event_time, value_type)
) PARTITION BY RANGE (event_date);

create index sensor_events_idx on sensor_events (event_date, event_time);
