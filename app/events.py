from flask import *
from flask_socketio import *
from app import socketio, games, lock
from .results import socketio_obj


@socketio.on('join')
def join(game_id):
    if game_id not in games.keys():
        return
    with lock:
        join_room(game_id)
        emit('results', socketio_obj(*games[game_id]), to=request.sid)
