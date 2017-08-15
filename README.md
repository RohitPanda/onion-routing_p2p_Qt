# voidphone-onion-qt
See reports for more documentation.

## Installation & Building

## Testing
### Unit tests
The code contains Qt Unit tests. To build tests uncomment line 3 in onion.pro and build again. Then run oniontest from the build folder. All testing classes are contained in the tests subdirectory.

### Marco/Polo
This test replaces real RPS and Onion Auth apis with mock ones, using the commandline parameters --mock-auth and --mock-peer. A peer started with --marco <peer> will build a tunnel to <peer> and start sending "marco" messages. Peers started with --polo, reply with "polo" upon receiving a "marco" message. The marco peer terminates the circuit after getting 100 responses. This successfully tests tunnel building, extending, destroying and data transfer.

#### Deployment (windows only)
On Windows, Qt Creator keeps Qt dlls outside the build folder. Thus the executable on its own won't start without additional work. Either include the Qt binary paths in your path (not recommended) or use the Qt deployment tool:
1. execute `Qt <version> for Desktop` item from your start menu. It opens a shell setup for the respective toolkit
2. cd into your build folder
3. execute `windeployqt.exe onion.exe`

#### Setup
In the code directory there are bootstrapping configs that start four nodes on localhost, with their P2P modules listening on ports 10001-10004. Now we have to start 3 of them as listening peers (--polo), and give them a mock peer (so they don't pollute log files that they cannot connect to the RPS api). The fourth peer needs the intermediate hops as mock peers, and the remaining one as target, thus he is started like `onion --mock-auth --mock-peer 127.0.0.1:10002 --mock-peer 127.0.0.1:10003 --polo --marco 127.0.0.1:10004 -c marcopolo1.conf`.

You can start the test with four terminals and lots of typing, or use the python script `python marcopolo.py <path to qt executable>`. The script redirects the log of the peers to log_i.txt. If you want to see data flowing through the hops, add a -v or --verbose flag to their commandline, e.g. by editing the args array in the python file.
