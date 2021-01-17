{* CAGRAPH.INC *}
{ https://oku.edu.mie-u.ac.jp/~okumura/algo/algo_pas.html }

const
  CMaxY = 240;

var
  LastX, LastY: Real;
  UseFlip: Boolean;


function FlipY(Y: Integer): Integer;
begin
  if UseFlip then
    FlipY := CMaxY - Y
  else
    FlipY := Y;
end;

procedure CA_SetFlipMode(Flip: Boolean);
begin
  UseFlip := Flip;
end;

procedure CA_Dot(X, Y: Integer);
begin
  DrawPixel(X, FlipY(Y));
end;

procedure CA_Line(X1, Y1, X2, Y2: Integer);
begin
  DrawLine(X1, FlipY(Y1), X2, FlipY(Y2));
end;

procedure CA_Circle(Xcenter, Ycenter, Radius: Integer);
begin
  DrawCircle(Xcenter, FlipY(Ycenter), Radius);
end;

procedure CA_MoveAbsolute(X, Y: Real);
begin
  LastX := X;
  LastY := Y;
end;

procedure CA_InitPlot;
begin
  CA_SetFlipMode(False);
  CA_MoveAbsolute(0, 0);
end;

procedure CA_DrawAbsolute(X, Y: Real);
begin
  CA_Line(Round(LastX), Round(LastY), Round(X), Round(Y));
  CA_MoveAbsolute(X, Y);
end;

procedure CA_MoveRelative(X, Y: Real);
begin
  CA_MoveAbsolute(LastX + X, LastY + Y);
end;

procedure CA_DrawRelative(X, Y: Real);
begin
  CA_DrawAbsolute(LastX + X, LastY + Y);
end;

procedure CA_DrawCircle(R: Real);
begin
  CA_Circle(Round(LastX), Round(LastY), Round(R));
end;
