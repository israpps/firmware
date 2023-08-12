# SD2PSXMAN PROTOCOL `0x01`

By el Isra, heavily based on the SD2PSX protocol drafts by BBSAN2k

> when we talk of _send_ and _respond_ this is written from the PS2 POV. so "sent byte" means PS2 -> SD2PSX

## Basic command structure
packet offset | sent byte | response byte | explanation |
:-----------: | :-------: | :-----------: | :---------: |
`0x00`        | `0x81`    | `0xFF`        | Sony standard initial byte. tells SIO2MAN we want to talk to memory card. SD2PSX ignores every packet if value is different
`0x01`        | `0xA0`    | `0xFF`        | we tell SD2PSX that who is talking is SD2PSXMAN, not MCMAN/SECRMAN/etc...
| ... | ... | ... | ...

to avoid making this read larger, the previous table only has the commands and responses that dont vary trough all the CMDs

Consider the following:

if offset `0x02` does not contain a valid CMD, the response will be `0xFF` instead of `0x00`, followed by `0x00`, then `0xFF` again at last byte.


## `0x20` - Ping
packet offset | sent byte | response byte | explanation |
:-----------: | :-------: | :-----------: | :---------: |
`0x02`        |`0x20`     | `0x00` | Ping Command |
`0x03`        |`0x00`     | `proto version` | current protocol version (1) |
`0x04`        |`0x00`     | `product id` | product ID of the card; <br>`0x01`: SD2PSX<br> (may be extended further with other cards using this protocol) |
`0x05`        | `0x00`    | `0x27` | SD2PSX answers here `0x27` |
`0x06`        | `0x00`    | `0xFF` | Termination Signal |

### `0x21` - Set GameID 
packet offset | sent byte | response byte | explanation |
:-----------: | :-------: | :-----------: | :---------: |
| `0x02` | `0x21` |`0x00`| GAMEID Command |
| `0x03` | `0x00` |`0x00`| padding? |
| `0x04` | `strlen` |`0x00`| Length of the game id transmitted (`strlen`) |
| `0x05`-`0xFF` | `0x00` for byte `0x05`, `0xFF` for byte `0x06` [^PACKETLEN] |`Prev Byte`| Game id data as string |

[^PACKETLEN]: To maintan unnecesary large buffers away of the IOP, any senseless response after 0x06 offset will not be made. meaning SIO2PACKET Response buffer size can be `0x07` all the time on SD2PSXMAN. TODO: CONFIRM IF THIS IS OK. OR IF SIO2 SPECS EXPECT A RESPONSE TO ALL BYTES (AKA: COMMAND AND RESPONSE OF SAME SIZE)

## `0x22` - Card channel change
packet offset | sent byte | response byte | explanation |
:-----------: | :-------: | :-----------: | :---------: |
`0x02`        | `0x22`    | `0x00`        | Card slot change command
`0x03`        | `0x0`, `0x1` or `0x2` | `0x00` | operation ID.</br>`0x0`: fixed value, assigns next byte as channel ID</br>`0x01`: next channel</br>`0x02`: previous channel
`0x04`        | channel slot (int) | `0x00`  | number of memory card channel to mount, usefull only of previous byte was `0x00`
`0x05` | `0x00` | `0x00` | padding
`0x06` |`0x00`| `0xFF` | Termination Signal |

## `0x23` - Card slot change
packet offset | sent byte | response byte | explanation |
:-----------: | :-------: | :-----------: | :---------: |
`0x02`        | `0x23`    | `0x00`        | Card slot change command
`0x03`        | `0x0`, `0x1` or `0x2` | `0x00` | operation ID.</br>`0x0`: fixed value, assigns next byte as card ID</br>`0x01`: next card</br>`0x02`: previous card
`0x04`        | card slot (int) | `0x00`  | number of memory card slot to mount, usefull only of previous byte was `0x00`
`0x05` | `0x00` | `0x00` | padding
`0x06` |`0x00`| `0xFF` | Termination Signal |



`` | `` | `` | 