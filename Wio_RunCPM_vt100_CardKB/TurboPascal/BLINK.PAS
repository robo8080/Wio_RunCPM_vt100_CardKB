program BLINK;
{$I ARDUINO.INC}
const
  LED_PIN = 0;
  function Conv(v: Integer): Integer;
  begin
    Conv := v * 5 div 3; { Wio Terminal 120MHz }  
  end;
begin
  pinMode(LED_PIN, OUTPUT);
  repeat
    digitalWrite(LED_PIN, HIGH);
    Delay(Conv(1000));
    digitalWrite(LED_PIN, LOW);
    Delay(Conv(1000));
  until KeyPressed;
end.