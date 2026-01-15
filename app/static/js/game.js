'use strict'

const socket = io.connect(window.location.origin);
const gameId = location.href.substring(location.href.lastIndexOf('/') + 1);

function showLeaderboard(players, populations, scores, final) {
    let n = players.length;
    let leaderboard = document.getElementById("leaderboard");
    while (leaderboard.children.length > 3) {
        leaderboard.removeChild(leaderboard.lastChild);
    }
    while (leaderboard.children.length < 3 * (n + 1)) {
        let child = document.createElement("div");
        child.classList.add("leaderboard-item");
        leaderboard.appendChild(child);
    }
    for (let i = 0; i < n; i++) {
        if (final && populations[i] > 0.66) {
            players[i] = "🏆 " + players[i];
        }
        leaderboard.children[3 + 3 * i].innerHTML = players[i];
        leaderboard.children[4 + 3 * i].innerHTML = populations[i];
        leaderboard.children[5 + 3 * i].innerHTML = scores[i];
    }
}

function finalize(error) {
    let download = document.getElementById("download");
    download.setAttribute("href", `${gameId}/download`);
    download.style.display = "";
    if (error !== null) {
        document.getElementById("show-error").style.display = "";
        document.getElementById("leaderboard-title").style.display = "none";
        document.getElementById("error-title").style.display = "";
        document.getElementById("error-content").innerText = error;
    } else {
        document.getElementById("leaderboard-title").innerHTML = "Final leaderboard";
    }
}

function showError() {
    document.getElementById("popup").style.display = "";
}

function hideError() {
    document.getElementById("popup").style.display = "none";
}

socket.on("connect", () => socket.emit("join", gameId))
socket.on("results", (message) => {
    let tournamentId = message.tournament_id;
    let numTournaments = message.n_tournaments;
    let matchId = message.match_id;
    let numMatches = message.n_matches;
    let error = message.results.error;
    if (tournamentId == -1) {
        document.getElementById("progress").innerHTML = "Compiling strategies...";
        if (error != null) {
            finalize(error);
        }
        return;
    }
    showLeaderboard(message.results.strats, message.results.populations, message.results.scores, tournamentId == numTournaments);
    //let percentage = Math.round(100.0 * tournamentId / numTournaments);
    document.getElementById("progress").innerHTML = `Tournament ${tournamentId} / ${numTournaments}, match ${matchId} / ${numMatches}`;
    if (error != null || tournamentId == numTournaments) {
        finalize(error);
    }
});

window.onload = () => {
    document.getElementById("show-error").onclick = showError;
    document.getElementById("hide-error").onclick = hideError;
}