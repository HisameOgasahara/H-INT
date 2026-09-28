import can
import cantools
import time
import random

bus = can.Bus(interface='kvaser', channel=1, bitrate=1000000)
db = cantools.database.load_file('project.dbc')
messages_to_send = [message for message in db.messages if 'ECU1' in message.senders]

for msg in messages_to_send:
    data = {sig.name: random.randint(sig.minimum or 0, sig.maximum or (2**sig.length - 1)) for sig in msg.signals}
    can_msg = can.Message(arbitration_id=msg.frame_id, data=msg.encode(data), is_extended_id=False)
    try:
        if (msg.send_type == 'Cyclic') and (msg.cycle_time is not None) and (msg.cycle_time > 0):
            bus.send_periodic(can_msg, msg.cycle_time / 1000)
        else:
            bus.send(can_msg)
        print(f"Message sent: {can_msg}")
    except can.CanError:
        print("Message NOT sent")

try:
    while True:
        time.sleep(0.1)
except KeyboardInterrupt:
    print("End of transmission")