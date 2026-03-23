create table sensor_events_aggregated (
  id integer not null primary key default NEXTVAL('sensor_events_aggregated_id_seq'),
  event_date integer not null,
  sensor_id smallint not null references sensors(id),
  value_type char(4) not null,
  min_value integer not null,
  max_value integer not null,
  avg_value integer not null,
  constraint sensor_events_aggregated_uk unique(sensor_id, event_date, value_type)
);
