drop table sensors;
create table sensors (
  id smallint not null primary key,
  name varchar(20) not null unique,
  data_type char(3) not null,
  location_id smallint not null references locations(id),
  device_id smallint,
  device_sensors device_sensor[],
  offsets sensor_offset[]
);
