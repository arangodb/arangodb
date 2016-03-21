var logs = document.getElementById("logs");

function printResult(str) {
  console.log(str);
  logs.innerHTML += str + '<br>';
}

function printError(str) {
  console.log(str);
  logs.innerHTML += str + '<br>';
}

function printScore(str) {
  console.log(str);
  logs.innerHTML += str + '<br>';
}

window.onload = function() {
  console.log('Running benchmarks.');
  benchmarks.runAll({notifyResult: printResult,
                     notifyError:  printError,
                     notifyScore:  printScore}, true);
  printResult('Benchmarks completed.');
}