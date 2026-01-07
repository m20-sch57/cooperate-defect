import threading
from flask import Flask
from flask_socketio import SocketIO


app = Flask(__name__)
socketio = SocketIO(app)

games = dict()
lock = threading.Lock()


from app import routes, events
