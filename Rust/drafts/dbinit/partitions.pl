$y = 2022;

while ($y < 2026) {
  for ($m = 2; $m <= 12; $m++) {
    $tm = $m;
    if ($tm < 10) {
      $tm = "0$tm";
    }
    $tpm = $m - 1;
    if ($tpm < 10) {
      $tpm = "0$tpm";
    }
    print "CREATE TABLE sensor_events_$y$tpm PARTITION OF sensor_events\n";
    print "FOR VALUES FROM ($y${tpm}01) TO ($y${tm}01);\n";
  }
  $py = $y;
  $y++;
  $tm="01";
  $tpm = 12;
  print "CREATE TABLE sensor_events_$py$tpm PARTITION OF sensor_events\n";
  print "FOR VALUES FROM ($py${tpm}01) TO ($y${tm}01);\n";
}
