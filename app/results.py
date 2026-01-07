class Results:
    def __init__(self, players, scores):
        strats = list(set(players))
        scores_grouped = {strat: [] for strat in strats}
        for player, score in zip(players, scores):
            scores_grouped[player].append(score)
        populations = [round(len(scores_grouped[strat]) / len(players) * 100) / 100 for strat in strats]
        scores = [self.median(scores_grouped[strat]) for strat in strats]
        arr = list(zip(populations, scores, strats))
        arr.sort(reverse=True)
        populations, scores, strats = zip(*arr) if arr else ([], [], [])
        self.populations = list(populations)
        self.scores = list(scores)
        self.strats = list(strats)
        self.error = None
    
    @staticmethod
    def median(arr):
        return sorted(arr)[len(arr) // 2]
    
    # @staticmethod
    # def tabify(s, l):
    #     return s + ' ' * max(0, l - len(s))

    # def __str__(self):
    #     res = [(self.scores[i] / self.max_scores[i] if self.max_scores[i] > 0 else 0, i) 
    #            for i in range(len(self.players))]
    #     res.sort(reverse=True)
    #     width = max([len(player) for player in self.players])
    #     s = ''
    #     for frac, i in res:
    #         name = self.tabify(self.players[i], width)
    #         s += f'{name}\t{frac:.3f}\t({self.scores[i]} / {self.max_scores[i]})\n'
    #     return s
    

def socketio_obj(tournament_id, n_tournaments, match_id, n_matches, results):
    return {
        'tournament_id': tournament_id,
        'n_tournaments': n_tournaments,
        'match_id': match_id,
        'n_matches': n_matches,
        'results': results.__dict__
    }
