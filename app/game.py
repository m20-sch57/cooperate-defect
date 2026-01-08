import os
import random
import shutil
import subprocess
from app import socketio, games, lock
from .results import Results, socketio_obj


def get_strats_dir(game_id):
    return os.path.join('games', game_id, 'strats')


def get_results_dir(game_id):
    return os.path.join('games', game_id, 'results')


def get_tournament_dir(game_id, tournament_id, n_tournaments):
    return os.path.join(get_results_dir(game_id), 'tournament' + str(tournament_id).zfill(len(str(n_tournaments - 1))))


def get_match_dir(game_id, tournament_id, n_tournaments, match_id, n_matches):
    return os.path.join(get_tournament_dir(game_id, tournament_id, n_tournaments), 'match' + str(match_id).zfill(len(str(n_matches - 1))))


def prepare_strats(strats_dir, sample_strats, user_strat_names, user_strat_contents):
    all_strats = []
    for strat_name in sample_strats:
        shutil.copyfile(os.path.join('sample_strats', strat_name),
                        os.path.join(strats_dir, strat_name))
        os.chmod(os.path.join(strats_dir, strat_name), 0o775)
        all_strats.append(strat_name)
    for strat_name, strat_content in zip(user_strat_names, user_strat_contents):
        fout = open(os.path.join(strats_dir, strat_name), 'wb')
        fout.write(strat_content)
        fout.close()
        os.chmod(os.path.join(strats_dir, strat_name), 0o775)
        all_strats.append(strat_name)
    return all_strats


def play_match(match_dir, strats_dir, strat1, strat2):
    os.makedirs(match_dir)
    strat1_full = os.path.join(os.getcwd(), strats_dir, strat1)
    strat2_full = os.path.join(os.getcwd(), strats_dir, strat2)
    master_path = os.path.join(os.getcwd(), 'master')
    config_path = os.path.join(os.getcwd(), match_dir, 'config.log')
    log_path = os.path.join(os.getcwd(), match_dir, 'match.log')
    user_path = os.path.join(os.getcwd(), match_dir, 'user.log')
    error_path = os.path.join(os.getcwd(), match_dir, 'error.log')
    proc = subprocess.Popen([master_path, config_path, log_path, user_path, error_path, strat1_full, strat2_full],
                            cwd=match_dir)
    status = proc.wait()
    return status == 0


def get_error(match_dir):
    try:
        return open(os.path.join(match_dir, 'error.log')).read()
    except FileNotFoundError:
        return ''


def start_game(game_id, n_tournaments, strat_players, sample_strats, user_strat_names, user_strat_contents):
    strats_dir = get_strats_dir(game_id)
    results_dir = get_results_dir(game_id)
    os.makedirs(strats_dir)
    os.makedirs(results_dir)
    strats = prepare_strats(strats_dir, sample_strats, user_strat_names, user_strat_contents)
    players = sum([[strat] * strat_players for strat in strats], [])
    last_scores = [0] * len(players)
    for tournament_id in range(n_tournaments):
        n = len(players)
        scores = [0] * n
        score_fn = {
            ('c', 'c'): (2, 2),
            ('c', 'd'): (-1, 3),
            ('d', 'c'): (3, -1),
            ('d', 'd'): (0, 0),
        }
        pairs = [(i, j) for i in range(n) for j in range(i)]
        n_matches = len(pairs)
        for match_id, (i, j) in enumerate(pairs):
            with lock:
                games[game_id] = (tournament_id, n_tournaments, match_id, n_matches, Results(players, last_scores))
                socketio.emit('results', socketio_obj(*games[game_id]), room=game_id)
            match_dir = get_match_dir(game_id, tournament_id, n_tournaments, match_id, n_matches)
            strat1, strat2 = players[i], players[j]
            success = play_match(match_dir, strats_dir, strat1, strat2)
            if not success:
                with lock:
                    games[game_id][-1].error = get_error(match_dir)
                    socketio.emit('results', socketio_obj(*games[game_id]), room=game_id)
                return
            with open(os.path.join(match_dir, 'match.log')) as fin:
                lines = fin.readlines()
            if not lines:
                with lock:
                    games[game_id][-1].error = get_error(match_dir)
                    socketio.emit('results', socketio_obj(*games[game_id]), room=game_id)
                return
            for line in lines:
                turn1, turn2 = line.split()
                score1, score2 = score_fn[turn1, turn2]
                scores[i] += score1
                scores[j] += score2
        arr = list(zip(scores, players))
        random.shuffle(arr)
        arr.sort(reverse=True, key=lambda x: x[0])
        cnt = int(round(n * 0.1))
        arr[n - cnt:] = arr[:cnt]
        last_scores, players = zip(*arr)
        if len(set(players)) == 1:
            break
    with lock:
        games[game_id] = (n_tournaments, n_tournaments, 0, 0, Results(players, last_scores))
        socketio.emit('results', socketio_obj(*games[game_id]), room=game_id)
    with open(os.path.join(results_dir, 'results.txt'), 'w') as fout:
        fout.write(str(games[game_id][-1]))
