use std::collections::HashMap;
use rand::rngs::OsRng;
use rand::TryRngCore;

#[derive(Clone)]
pub struct Aggregated {
    pub min: i32,
    pub avg: i32,
    pub max: i32
}

impl Aggregated {
    fn new() -> Aggregated {
        Aggregated{min: i32::MAX, avg: 0, max: i32::MIN}
    }

    fn random() -> Aggregated {
        let v = (OsRng.try_next_u32().unwrap() / 10) as i32;
        Aggregated{min: v, avg: v * 2, max: v * 4}
    }

    fn add(&mut self, value: &Aggregated) {
        self.avg += value.avg;
        if value.min < self.min {
            self.min = value.min;
        }
        if value.max > self.max {
            self.max = value.max;
        }
    }

    fn calculate_average(&mut self, n: i32) {
        self.avg /= n;
    }
}

#[derive(Clone)]
pub struct SensorDataValues {
    pub time: i32,
    pub values: HashMap<String, i32>
}

impl SensorDataValues {
    fn add(&mut self, values: &HashMap<String, i32>) {
        for (key, value) in values {
            *self.values.entry(key.clone()).or_insert(0) += value;
        }
    }

    fn calculate_average(&mut self, n: i32, new_time: i32) {
        self.values = self.values.iter().map(|(k, v)| (k.clone(), *v / n)).collect();
        self.time = new_time;
    }
    
    fn to_binary(&self, result: &mut Vec<u8>) {
        result.push(self.values.len() as u8);
        result.extend_from_slice(&self.time.to_le_bytes());
        for (key, value) in &self.values {
            append_key(key, result);
            result.extend_from_slice(&value.to_le_bytes());
        }
    }
}

#[derive(Clone)]
pub struct SensorData {
    pub date: i32,
    pub values: Option<SensorDataValues>,
    pub aggregated: Option<HashMap<String, Aggregated>>
}

pub struct SensorDataOut {
    date: i32,
    values: Option<Vec<SensorDataValues>>,
    aggregated: Option<HashMap<String, Aggregated>>
}

impl SensorData {
    fn from(source: &SensorData) -> SensorData {
        SensorData{date: source.date, values: source.values.as_ref().map(|v|v.clone()),
            aggregated: source.aggregated.as_ref().map(|v|v.clone())}
    }

    fn add(&mut self, data: &SensorData) {
        if let Some(values) = &mut self.values {
            values.add(&data.values.as_ref().unwrap().values);
        }
        if let Some(aggregated) = &mut self.aggregated {
            for (key, value) in data.aggregated.as_ref().unwrap() {
                aggregated.entry(key.clone()).or_insert(Aggregated::new()).add(value);
            }
        }
    }

    fn calculate_average(&mut self, n: i32, new_time: i32, new_date: i32) {
        self.date = new_date;
        if let Some(values) = &mut self.values {
            values.calculate_average(n, new_time);
        }
        if let Some(aggregated) = &mut self.aggregated {
            for (_key, value) in aggregated {
                value.calculate_average(n);
            }
        }
    }

    pub fn to_binary(&self) -> Vec<u8> {
        let mut result = Vec::new();
        result.extend_from_slice(&self.date.to_le_bytes());
        if let Some(value) = &self.values {
            value.to_binary(&mut result);
        }
        result
    }
}

impl SensorDataOut {
    pub fn to_binary(&self) -> Vec<u8> {
        let mut result = Vec::new();
        result.extend_from_slice(&self.date.to_le_bytes());
        if let Some(values) = &self.values {
            result.extend_from_slice(&(values.len() as u32).to_le_bytes());
            for value in values {
                value.to_binary(&mut result);
            }
        }
        if let Some(aggregated) = &self.aggregated {
            result.push(aggregated.len() as u8);
            for (key, value) in aggregated {
                append_key(key, &mut result);
                result.extend_from_slice(&value.min.to_le_bytes());
                result.extend_from_slice(&value.avg.to_le_bytes());
                result.extend_from_slice(&value.max.to_le_bytes());
            }
        }
        result
    }
}

fn append_key(key: &String, result: &mut Vec<u8>) {
    let bytes = key.as_bytes();
    result.push(bytes[0]);
    result.push(bytes[1]);
    result.push(bytes[2]);
    result.push(if bytes.len() > 3 { bytes[3] } else { 0x20 });
}

pub fn aggregate_by_max_points(data: Vec<SensorData>, max_points: usize, aggregated: bool)
                           -> Vec<SensorDataOut> {
    let factor = data.len() / max_points + 1;
    let mut idx = 0;
    let mut result = Vec::new();
    while idx < data.len() {
        let start = idx;
        let mut sd = SensorData::from(data.get(idx).unwrap());
        idx += 1;
        for _i in 1..factor {
            if idx >= data.len() {
                break;
            }
            sd.add(data.get(idx).unwrap());
            idx += 1;
        }
        let n = idx - start;
        let d = data.get(start + n / 2).unwrap();
        sd.calculate_average(n as i32,
                             d.values.as_ref().map(|v|v.time).unwrap_or(0), d.date);
        result.push(sd);
    }
    to_sensor_data_out(result, aggregated)
}

fn to_sensor_data_out(data: Vec<SensorData>, aggregated: bool) -> Vec<SensorDataOut> {
    let mut map = HashMap::new();
    for d in data {
        let day_data = map.entry(d.date).or_insert(Vec::new());
        day_data.push(d);
    }
    let mut v: Vec<SensorDataOut> = map.into_iter().map(|(k, v)|SensorDataOut{
        date: k,
        aggregated: if aggregated { v[0].aggregated.clone() } else { None },
        values: if aggregated { None } else { Some(aggregate_values(v)) },
    }).collect();
    v.sort_by(|a,b|a.date.cmp(&b.date));
    v
}

fn aggregate_values(values: Vec<SensorData>) -> Vec<SensorDataValues> {
    values.into_iter().map(|v|v.values.unwrap()).collect()
}

#[cfg(test)]
mod tests {
    use std::collections::HashMap;
    use rand::rngs::OsRng;
    use rand::TryRngCore;
    use crate::sensor_data::{aggregate_by_max_points, Aggregated, SensorData, SensorDataValues};

    fn generate_values(time: i32, value_types: &[&str]) -> SensorDataValues {
        SensorDataValues{time,
            values: value_types.into_iter().map(|v|(v.to_string(), OsRng.try_next_u32().unwrap() as i32)).collect()}
    }

    #[test]
    fn test_aggregate_by_max_points_raw() {
        let source = vec![
            SensorData{date:20250331, values: Some(generate_values(100000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(100500, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(101000, &["humi", "pres", "temp", "co2"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(101500, &["humi", "pres", "temp", "co2"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(102000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(102500, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(103000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(103500, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(104000, &["humi", "pres", "temp"])), aggregated: None},
            SensorData{date:20250331, values: Some(generate_values(104500, &["humi", "pres", "temp"])), aggregated: None}
        ];
        let mut result = aggregate_by_max_points(source.clone(), 3, false);
        assert_eq!(1, result.len());
        let data = result.remove(0);
        assert_eq!(data.date, 20250331);
        assert!(data.aggregated.is_none());
        assert!(data.values.is_some());
        let mut values = data.values.unwrap();
        assert_eq!(3, values.len());
        let value = values.remove(0);
        assert_eq!(value.time, 101000);
        check_raw_values(value.values, &source[0..4]);
        let value = values.remove(0);
        assert_eq!(value.time, 103000);
        check_raw_values(value.values, &source[4..8]);
        let value = values.remove(0);
        assert_eq!(value.time, 104500);
        check_raw_values(value.values, &source[8..10]);
    }

    fn check_raw_values(values: HashMap<String, i32>, data: &[SensorData]) {
        let mut average = HashMap::new();
        for value in data {
            for (key, data) in &value.values.as_ref().unwrap().values {
                *average.entry(key.clone()).or_insert(0) += *data;
            }
        }
        average = average.into_iter().map(|(k, v)| (k, v / (data.len() as i32))).collect();
        assert_eq!(average.len(), values.len());
        for (k, v) in average {
            assert_eq!(*values.get(&k).unwrap(), v);
        }
    }

    fn generate_aggregated(value_types: &[&str]) -> HashMap<String, Aggregated> {
        value_types.into_iter().map(|v|(v.to_string(), Aggregated::random())).collect()
    }

    #[test]
    fn test_aggregate_by_max_points_aggregated() {
        let source = vec![
            SensorData{date:20250331, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250401, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250402, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp", "co2"]))},
            SensorData{date:20250403, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp", "co2"]))},
            SensorData{date:20250404, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250405, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250406, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250407, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250408, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
            SensorData{date:20250409, values: None, aggregated: Some(generate_aggregated(&["humi", "pres", "temp"]))},
        ];
        let mut result = aggregate_by_max_points(source.clone(), 3, true);
        assert_eq!(3, result.len());
        let data = result.remove(0);
        assert_eq!(data.date, 20250402);
        assert!(data.aggregated.is_some());
        assert!(data.values.is_none());
        check_aggregated_values(data.aggregated.unwrap(), &source[0..4]);
        let data = result.remove(0);
        assert_eq!(data.date, 20250406);
        assert!(data.aggregated.is_some());
        assert!(data.values.is_none());
        check_aggregated_values(data.aggregated.unwrap(), &source[4..8]);
        let data = result.remove(0);
        assert_eq!(data.date, 20250409);
        assert!(data.aggregated.is_some());
        assert!(data.values.is_none());
        check_aggregated_values(data.aggregated.unwrap(), &source[8..10]);
    }

    fn check_aggregated_values(values: HashMap<String, Aggregated>, data: &[SensorData]) {
        let mut average = HashMap::new();
        for value in data {
            for (key, data) in value.aggregated.as_ref().unwrap() {
                average.entry(key.clone()).or_insert(Aggregated::new()).add(data);
            }
        }
        for (_key, agg) in &mut average {
            agg.calculate_average(data.len() as i32);
        }
        assert_eq!(average.len(), values.len());
        for (k, a) in average {
            let v = values.get(&k).unwrap();
            assert_eq!(v.min, a.min);
            assert_eq!(v.avg, a.avg);
            assert_eq!(v.max, a.max);
        }
    }

    #[test]
    fn test_sensor_data_values() {
        let values = SensorDataValues{time: 112233, values: HashMap::from([
            ("temp".to_string(), 555),
            ("humi".to_string(), 777),
            ("lux".to_string(), 999)
        ])};
        let mut binary = Vec::new();
        values.to_binary(&mut binary);
        assert_eq!(binary.len(), 29);
        let count = binary[0];
        assert_eq!(count, 3);
        let mut buffer32 = [0u8; 4];
        buffer32.copy_from_slice(&binary[1..5]);
        let time = i32::from_le_bytes(buffer32);
        assert_eq!(time, 112233);
        let mut idx = 5;
        for _i in 0..values.values.len() {
            let key = String::from_utf8(binary[idx..idx+4].to_vec()).unwrap().trim().to_string();
            idx += 4;
            buffer32.copy_from_slice(&binary[idx..idx+4]);
            idx += 4;
            let value = i32::from_le_bytes(buffer32);
            assert_eq!(values.values[&key], value);
        }
    }
}
