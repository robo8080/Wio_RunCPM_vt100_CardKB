program RTCTEST;
{$I ARDUINO.INC}
var
  Year: Integer;
  Month, Day, Hour, Minute, Second: Byte;
begin
  Write('Year : ');
  Readln(Year);
  Write('Month: ');
  Readln(Month);
  Write('Day  : ');
  Readln(Day);
  Writeln;
  Write('Hour   : ');
  Readln(Hour);
  Write('Minute : ');
  Readln(Minute);
  Write('Second : ');
  Readln(Second);
  Writeln;
  setDate(Year, Month, Day);
  setTime(Hour, Minute, Second);
  repeat
    getDate(Year, Month, Day);
    getTime(Hour, Minute, Second);
    Writeln(Year, '/', Month, '/', Day, ' ', Hour, ':', Minute, ':', Second);
    Delay(10000);
  until KeyPressed;
end.
