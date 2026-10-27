# NMEA parser

NMEA 0183 sentences look like

| `$` | talker (2 chars) + type (3 chars) | `,field,field,...` | `*` | checksum (2 hex chars) | `\r\n` |
|-----|-----------------------------------|--------------------|-----|------------------------|--------|

e.g., `$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47`, where the checksum
is the XOR of all characters between `$` and `*`. The checksum is mandatory, and either `\r` or
`\n` ends the sentence.

The receive state machine is shown below. Each state waits for a character: `Evaluate()` peeks
at it to choose the transition, and `Exit()` consumes it. A `$` anywhere restarts the sentence,
so a truncated sentence doesn't hide the one following it. Other unexpected characters, or a
too long sentence, drop the sentence.

`Complete` parses the sentence, and merges the data with other sentences in the same
`PushData()` call.

```mermaid
stateDiagram-v2
    [*] --> WaitForDollar

    WaitForDollar --> Sentence : $
    WaitForDollar --> WaitForDollar : Other character

    Sentence --> Checksum0 : *
    Sentence --> Sentence : $ (restart)
    Sentence --> Sentence : Printable character
    Sentence --> WaitForDollar : Other character, or sentence full

    Checksum0 --> Checksum1 : Hex digit
    Checksum0 --> Sentence : $ (restart)
    Checksum0 --> WaitForDollar : Other character

    Checksum1 --> EndOfLine : Hex digit
    Checksum1 --> Sentence : $ (restart)
    Checksum1 --> WaitForDollar : Other character

    EndOfLine --> Complete : \r or \n, valid checksum
    EndOfLine --> Sentence : $ (restart)
    EndOfLine --> WaitForDollar : Other character, or invalid checksum

    Complete --> WaitForDollar
```
