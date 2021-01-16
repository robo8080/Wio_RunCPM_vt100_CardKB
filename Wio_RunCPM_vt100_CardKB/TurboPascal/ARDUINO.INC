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
  end; {of procedure digitalRead}

  procedure digitalWrite(pin, value: Byte);
  begin
    BDos(222, (pin shl 8) + value);
  end; {of procedure digitalWrite}

  function analogRead(pin: Byte): Integer;
  begin
    analogRead := BDosHL(223, (pin shl 8));
  end; {of procedure analogRead}

  procedure analogWrite(pin, value: Byte);
  begin
    BDos(224, (pin shl 8) + value);
  end; {of procedure analogWrite}

  function hostOS(pin, value: Byte): Integer;
  begin
    hostOS := BDosHL(225, (pin shl 8) + value);
  end; {of procedure hostOS}