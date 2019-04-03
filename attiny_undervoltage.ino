void setup() {
  pinMode(0, OUTPUT);
  analogReference(INTERNAL);
  delay(2000);
}

void loop() {
  uint16_t  i = analogRead(2);
  digitalWrite(0, i >= 890);
  if(!(i >= 890)){
    delay(5000);
  }
}
