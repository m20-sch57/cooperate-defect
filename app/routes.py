import os
import shutil
import uuid
from flask import *
from app import app, socketio, games, lock
from .game import start_game
from .results import Results


@app.route('/')
def new_game():
    return render_template('new_game.html')


@app.route('/game/<game_id>')
def view_game(game_id):
    if game_id not in games.keys():
        abort(404)
    return render_template('game.html')

@app.route('/game/<game_id>/download')
def download_game(game_id):
    if game_id not in games.keys():
        abort(404)
    game_dir = os.path.join('games', game_id)
    output_file = os.path.join('games', game_id)
    shutil.make_archive(output_file, 'zip', game_dir)
    return send_file(os.path.join(os.getcwd(), output_file + '.zip'), as_attachment=True)

@app.route('/submit', methods=['POST'])
def submit():
    n_tournaments = int(request.form['n-tournaments'])
    strat_players = int(request.form['strat-players'])
    sample_strats = request.form.getlist('sample-strats')
    strat_files = request.files.getlist('strat-files')
    if len(strat_files) + len(sample_strats) == 0:
        abort(400)
    game_id = str(uuid.uuid4())
    with lock:
        games[game_id] = (0, n_tournaments, 0, 0, Results([], []))
    user_strat_names = [strat_file.filename for strat_file in strat_files]
    user_strat_contents = [strat_file.read() for strat_file in strat_files]
    thread = socketio.start_background_task(start_game, game_id, n_tournaments, strat_players, 
                                            sample_strats, user_strat_names, user_strat_contents)
    return game_id
