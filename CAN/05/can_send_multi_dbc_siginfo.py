import can
import cantools
import time
import random

can_bus = can.Bus(interface='kvaser', channel=1, bitrate=1000000)
can_db = cantools.database.load_file('project.dbc')
messages_to_send = [message for message in can_db.messages if 'ECU1' in message.senders]

while True:
    for msg in messages_to_send:
        data = {}
        for sig in msg.signals:
            sig_info = {
                'name': sig.name, 'start': sig.start, 'length': sig.length,
                'byte_order': 'little_endian' if sig.byte_order == 'little_endian' else 'big_endian',
                'minimum': sig.minimum, 'maximum': sig.maximum
            }
            print(f"Signal: {sig_info}")

            value = random.randint(sig.minimum or 0, sig.maximum or (2**sig.length - 1))
            data[sig.name] = value

        can_msg = can.Message(arbitration_id=msg.frame_id, data=msg.encode(data), is_extended_id=False)

        try:
            can_bus.send(can_msg)
            print(f"Message sent: {can_msg}")
        except can.CanError:
            print("Message NOT sent")
        time.sleep(0.1)