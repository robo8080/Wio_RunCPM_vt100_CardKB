{* ARDUINO.INC *}

const
  INPUT        = 0; {ESP32: 1}
  OUTPUT       = 1; {ESP32: 2}
  INPUT_PULLUP = 2; {ESP32: 5}

  LOW  = 0;
  HIGH = 1;

  procedure pinMode(pin, mode: Byte);
  begin
    Bdos(220, (pin shl 8) + mode);
  end; {of procedure pinMode}

  function digitalRead(pin: Byte): Byte;
  begin
    digitalRead := BDos(221, (pin shl 8));
  end; {of function digitalRead}

  procedure digitalWrite(pin, value: Byte);
  begin
    BDos(222, (pin shl 8) + value);
  end; {of procedure digitalWrite}

  function analogRead(pin: Byte): Integer;
  begin
    analogRead := BDosHL(223, (pin shl 8));
  end; {of function analogRead}

  procedure analogWrite(pin, value: Byte);
  begin
    BDos(224, (pin shl 8) + value);
  end; {of procedure analogWrite}

  function hostOS(pin, value: Byte): Integer;
  begin
    hostOS := BDosHL(225, (pin shl 8) + value);
  end; {of function hostOS}

  procedure getDate(var y: Integer; var m, d: byte);
  var
    HL: Integer;
  begin
    y  := BDosHL($2A, 0 shl 14);
    HL := BDosHL($2A, 1 shl 14);
    m  := HL shr 8;
    d  := HL and $FF;
  end; {of procedure getDate}

  function getDayOfTheWeek: byte;
  begin
    getDayOfTheWeek := BDosHL($2A, 2 shl 14);
  end; {of function getDayOfTheWeek}

  procedure setDate(y: Integer; m, d: byte);
  var
    HL: Integer;
  begin
    HL := BDosHL($2B, (0 shl 14) or y);
    HL := BDosHL($2B, (1 shl 14) or (m shl 8) or d);
  end; {of procedure setDate}

  procedure getTime(var h, m, s: byte);
  var
    HL: Integer;
  begin
    HL := BDosHL($2C, 0 shl 14);
    h  := HL shr 8;
    m  := HL and $FF;
    HL := BDosHL($2C, 1 shl 14);
    s  := HL shr 8;
  end; {of procedure getTime}

  procedure setTime(h, m, s: byte);
  var
    HL: Integer;
  begin
    HL := BDosHL($2D, (0 shl 14) or (h shl 8) or m);
    HL := BDosHL($2D, (1 shl 14) or (s shl 8));
  end; {of procedure setTime}
