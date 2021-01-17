program LORENZ;
{$I EGR.INC}
{$I CAGRAPH.INC}
const
  A = 10.0;
  B = 28.0;
  C = 2.6666; {8.0 / 3.0}
  D = 0.01;
  Scale = 4.5;
var
  k: Integer;
  x, y, z, dx, dy, dz: Real;
begin
  InitGraph;
  CA_InitPlot;
  x := 1; y := 1; z := 1;
  for k := 0 to 2999 do
    begin
      dx := A * (y - x);
      dy := x * (B - z) - y;
      dz := x * y - C * z;
      x := x + D * dx;
      y := y + D * dy;
      z := z + D * dz;
      if k > 100 then
        CA_DrawAbsolute((x + 30) * Scale, z * Scale)
      else
        CA_MoveAbsolute((x + 30) * Scale, z * Scale);
    end;
  Readln;
end.
