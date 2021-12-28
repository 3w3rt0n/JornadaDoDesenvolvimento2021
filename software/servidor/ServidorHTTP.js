var fs = require("file-system");
const http = require("http");
const server = http.createServer();
//const palavra = "Outros"
const palavra = "Franzininho"
//const palavra = "Ligar"
//const palavra = "Desligar"
const fileNameRAW = "./resources/"+palavra+"/RAW/";
const fileNameCSV = "./resources/"+palavra+"/CSV/";

if (!fs.existsSync(fileName)){
    fs.mkdirSync(fileName);
}
if (!fs.existsSync(fileNameRAW)){
    fs.mkdirSync(fileNameRAW);
}
if (!fs.existsSync(fileNameCSV)){
    fs.mkdirSync(fileNameCSV);
}

const fs1 = require('fs')
var fileRAW = fs1.readdirSync(fileNameRAW).length + 1;
var fileCSV = fs1.readdirSync(fileNameCSV).length + 1;

console.log(fileRAW + " - Arquivos RAW.");
console.log(fileCSV + " - Arquivos CSV.");

server.on("request", (request, response) => {
	if (request.method == "POST" && request.url === "/uploadRAW") {
		var recordingFile = fs.createWriteStream(fileNameRAW + palavra + fileRAW + ".raw", { encoding: "utf8" });		
		request.on("data", function(data) {
			recordingFile.write(data);
		});

		request.on("end", async function() {
			recordingFile.end();
			response.writeHead(200, { "Content-Type": "text/plain" });
			console.log("Arquivo RAW " + fileRAW + " recebido!");
			response.end("ok!");
			fileRAW += 1;
		});
	}else if (request.method == "POST" && request.url === "/uploadCSV") {
		var recordingFile = fs.createWriteStream(fileNameCSV + palavra + fileCSV + ".csv", { encoding: "utf8" });		
		request.on("data", function(data) {
			recordingFile.write(data);
		});

		request.on("end", async function() {
			recordingFile.end();
			response.writeHead(200, { "Content-Type": "text/plain" });
			console.log("Arquivo CSV " + fileCSV + " recebido!");
			response.end("ok!");
			fileCSV += 1;
		});
	} else {
		console.log("Error Check your POST request");
		response.writeHead(405, { "Content-Type": "text/plain" });
	}
});

const port = 8888;
server.listen(port);
console.log(`Listening at ${port}`);
