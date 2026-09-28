import can
import cantools
import time
import random
import threading


def send_cyclic_msg(msg):
    while True:
        data = {sig.name: random.randint(sig.minimum or 0, sig.maximum or (2**sig.length - 1)) for sig in msg.signals}
        can_msg = can.Message(arbitration_id=msg.frame_id, data=msg.encode(data), is_extended_id=False)
        interval = msg.cycle_time / 1000.0
        try:
            can_bus.send(can_msg)
            print(f"Message sent: {can_msg}")
        except can.CanError:
            print("Message NOT sent")
        time.sleep(interval)


def send_msg(msg):
    data = {sig.name: random.randint(sig.minimum or 0, sig.maximum or (2**sig.length - 1)) for sig in msg.signals}
    can_msg = can.Message(arbitration_id=msg.frame_id, data=msg.encode(data), is_extended_id=False)
    try:
        can_bus.send(can_msg)
        print(f"Message sent: {can_msg}")
    except can.CanError:
        print("Message NOT sent")


can_bus = can.Bus(interface='kvaser', channel=1, bitrate=1000000)
can_db = cantools.database.load_file('project.dbc')
messages_to_send = [message for message in can_db.messages if 'ECU1' in message.senders]

for msg in messages_to_send:
    if (msg.send_type == 'Cyclic') and (msg.cycle_time is not None) and (msg.cycle_time > 0):
        thread_msg = threading.Thread(target=send_cyclic_msg, args=(msg,))
        thread_msg.daemon = True
    else:
        thread_msg = threading.Thread(target=send_msg, args=(msg,))
    thread_msg.start()

try:
    while True:
        time.sleep(0.1)
except KeyboardInterrupt:
    print("End of transmission")