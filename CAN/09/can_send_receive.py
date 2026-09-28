import can
import cantools
import time
import random
import threading
import sys

filters = [
    {"can_id": 0x001, "can_mask": 0xFFF, "extended": False},
    {"can_id": 0x005, "can_mask": 0x00F, "extended": False},
    {"can_id": 0x100, "can_mask": 0xFF0, "extended": False},
    {"can_id": 0x400, "can_mask": 0xF00, "extended": False},
    {"can_id": 0x501, "can_mask": 0xFF1, "extended": False},
    {"can_id": 0x602, "can_mask": 0xFFF, "extended": False},
    {"can_id": 0x700, "can_mask": 0xFF2, "extended": False},
]

can_bus = can.ThreadSafeBus(interface='kvaser', channel=sys.argv[1], bitrate=1000000, can_filters=filters)
can.BitTiming(f_clock=16_000_000, brp=2, tseg1=5, tseg2=2, sjw=2)

def recv_msg():
    while True:
        msg = can_bus.recv()
        if msg:
            print(f"Message received: {msg}")

def send_msg():
    can_db = cantools.database.load_file('project.dbc')
    messages_to_send = [message for message in can_db.messages if 'ECU1' in message.senders]
    for msg in messages_to_send:
        data = {sig.name: random.randint(sig.minimum or 0, sig.maximum or (2**sig.length - 1)) for sig in msg.signals}
        can_msg = can.Message(arbitration_id = msg.frame_id, data=msg.encode(data), is_extended_id=False)
        try:
            if (msg.send_type == 'Cyclic') and (msg.cycle_time is not None) and (msg.cycle_time > 0):
                can_bus.send_periodic(can_msg, msg.cycle_time / 1000)
            else:
                can_bus.send(can_msg)
            print(f"Message sent: {can_msg}")
        except can.CanError:
            print("Message NOT sent")

thread_msg_rx = threading.Thread(target=recv_msg, args=())
thread_msg_rx.daemon = True

thread_msg_tx = threading.Thread(target=send_msg, args=())
thread_msg_tx.daemon = True

thread_msg_rx.start()
time.sleep(5)
thread_msg_tx.start()

try:
    while True:
        time.sleep(0.1)
except KeyboardInterrupt:
    print("End of transmission")