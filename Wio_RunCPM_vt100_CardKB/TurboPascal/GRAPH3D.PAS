program GRAPH3D;
{$I EGR.INC}
{$I CAGRAPH.INC}
const
  Xmin = -1;
  Xmax = 1;
  Zmin = -1;
  Zmax = 1;
  Yscale = 100;
var
  X, Z: real;
  I, Ix, Iz, Px, Py: integer;
  Ok, LastOk: boolean;
  LowerHorizon, UpperHorizon: array [0..240] of integer;

  function Func(X, Z: real): real;
  begin
    Func := cos(10 * sqrt(sqr(X) + sqr(Z)))
  end; { Func }

begin
  InitGraph;
  CA_InitPlot;
  CA_SetFlipMode(True);
  for I := 0 to 240 do
  begin
    LowerHorizon[I] := maxint;
    UpperHorizon[I] := -maxint
  end;
  for Iz := 0 to 20 do
  begin
    Z := Zmin + (Zmax - Zmin) * Iz / 20;
    LastOk := false;
    for Ix := 0 to 200 do
    begin
      X := Xmin + (Xmax - Xmin) * Ix / 200;
      I := Ix + 2 * (20 - Iz);
      Px := 10 * I;
      Py := round(Yscale * Func(X, Z)) + 30 * Iz + 600;
      Ok := false;
      if Py < LowerHorizon[I] then
      begin
        LowerHorizon[I] := Py;
        Ok := true
      end;
      if Py > UpperHorizon[I] then
      begin
        UpperHorizon[I] := Py;
        Ok := true
      end;
      if Ok and LastOk then
        CA_DrawAbsolute(Px div 8, Py div 8)
      else
        CA_MoveAbsolute(Px div 8, Py div 8);
      LastOk := Ok
    end
  end
end.
