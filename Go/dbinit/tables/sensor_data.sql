drop table sensor_data;
create table sensor_data (
  id integer not null default NEXTVAL('sensor_data_id_seq') primary key,
  sensor_id integer not null references sensors(id),
  event_date date not null,
  event_time time not null,
  data json,
  error_message varchar(50),
  constraint sensor_data_uk unique(sensor_id, event_date, event_time)
);
