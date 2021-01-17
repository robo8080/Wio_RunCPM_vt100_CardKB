program BEEPTEST;
{$I EGR.INC}
var
  i: Integer;
  function Conv(v: Integer): Integer;
  begin
    Conv := v * 5 div 3; { Wio Terminal 120MHz }  
  end;
begin
  for i:=255 downto 1 do
    Beep(1, i, 800 div (i + 100));
  Delay(Conv(500));
  Beep(2, 50, 100);
  Delay(Conv(500));
  Beep(2, 50, 100);
  Delay(Conv(500));
  Beep(2, 50, 100);
  Delay(Conv(1500));
  Beep(3, 50, 100);
  Delay(Conv(1500));
  Beep(5, 55, 100);
end.
