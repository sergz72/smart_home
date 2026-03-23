drop table locations;
create table locations (
  id smallint not null primary key,
  name varchar(30) not null unique,
  location_type char(3) not null default 'int'
);
