'use strict'

const strats = [
    "cooperate.py", "defect.py", "rand.py"
];

const maxStrats = 12;

function fileOnChange() {
    let file = this.files[0];
    if (file === undefined) {
        return;
    }
    let flLabel = this.parentNode.querySelector("label + label");
    flLabel.innerHTML = file.name;
}

function selOnChange() {
    let val = this.value;
    let fl = this.parentNode.querySelector(".file");
    if (val !== "upload") {
        fl.style.display = "none";
    } else {
        fl.style.display = "";
    }
}

function buildStrat(num) {
    let strat = document.createElement("div");
    strat.classList.add("strat");
    let sel = document.createElement("select");
    sel.classList.add("select", "btn-bordered", "btn-white");
    sel.onchange = selOnChange;
    let opts = ["upload"].concat(strats);
    for (let optVal of opts) {
        let opt = document.createElement("option");
        opt.setAttribute("value", optVal);
        opt.innerHTML = optVal;
        sel.appendChild(opt);
    }
    let fl = document.createElement("div");
    fl.classList.add("file");
    let flInput = document.createElement("input");
    flInput.setAttribute("type", "file");
    flInput.setAttribute("id", `strat${num}`);
    flInput.onchange = fileOnChange;
    let flLabel1 = document.createElement("label");
    flLabel1.setAttribute("for", `strat${num}`);
    flLabel1.classList.add("btn", "btn-blue");
    flLabel1.innerHTML = "Choose file";
    let flLabel2 = document.createElement("label");
    flLabel2.setAttribute("for", `strat${num}`);
    flLabel2.innerHTML = "No file chosen";
    fl.appendChild(flInput);
    fl.appendChild(flLabel1);
    fl.appendChild(flLabel2);
    strat.appendChild(sel);
    strat.appendChild(fl);
    return strat;
}

async function submitStrats() {
    let formData = new FormData();
    formData.append("n-tournaments", document.getElementById("n-tournaments").value);
    formData.append("strat-players", document.getElementById("strat-players").value);
    let strats_grid = document.querySelector(".strats-grid");
    for (let child of strats_grid.children) {
        let sel = child.querySelector(".select");
        let inp = child.querySelector(".file input");
        if (sel.value === "upload") {
            if (inp.files.length > 0) {
                formData.append("strat-files", inp.files[0]);
            }
        } else {
            formData.append("sample-strats", sel.value);
        }
    }
    let response = await fetch("/submit", {
        method: "POST",
        body: formData
    });
    if (response.ok) {
        let id = await response.text();
        window.location.href = `game/${id}`;
    }
}

window.onload = () => {
    let strats_grid = document.querySelector(".strats-grid");
    for (let num = 0; num < maxStrats; ++num) {
        strats_grid.appendChild(buildStrat(num));
    }
    let submitBtn = document.querySelector(".footer button");
    submitBtn.onclick = submitStrats;
}