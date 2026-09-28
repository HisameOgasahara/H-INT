import can
import cantools
import time
import random

can_bus = can.Bus(interface='kvaser', channel=1, bitrate=1000000)
can_db = cantools.database.load_file('project.dbc')
messages_to_send = [message for message in can_db.messages if 'ECU1' in message.senders]

while True:
  for msg in messages_to_send:
    data = {sig.name: random.randint(sig.minimum or 0, sig.maximum or (2**sig.length - 1)) for sig in msg.signals}
    can_msg = can.Message(arbitration_id = msg.frame_id, data=msg.encode(data), is_extended_id=False)
    try:
      can_bus.send(can_msg)
      print(f"Message sent: {can_msg}")
    except can.CanError:
      print("Message NOT sent")
  time.sleep(0.1)