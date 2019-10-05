///////////////////////////////////////////////////////////////////////////////
// bix-blockchain
// Copyright (C) 2019 HUBECN LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version. See LICENSE.txt 
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.


// jsvar.debuglevel = 9999;

var activeMining = null;

function miningLedGreen() {
	document.getElementById('miningLed').className = 'led-green';
}
function miningLedRed() {
	document.getElementById('miningLed').className = 'led-red';
}

function jsMinerIncrementNonce(non) {
	if (non == "") return('0');
	var n = non.charAt(0);
	var app = non.substring(1);
	var res = '';
	if (n == '9') res = 'A' + app;
	else if (n == 'Z') res = 'a' + app;
	else if (n == 'z') res = '0' + jsMinerIncrementNonce(app);
	else res = String.fromCharCode(n.charCodeAt(0)+1) + app;
	return(res);
}

function jsMiningGenerateBlockCandidate() {
	if (activeMining == null) return(null);

	var i = activeMining.tokenIndex;
	if (i >= minerTokens.length) return(null);
	var block =  minerTokens[i].minedblock;
	if (block == null || block == "") return(null);
	var mym = "mvl=Mining;acc="+activeMining.account+";";
	var mss = block+"mid=mining;tsu="+new Date().getTime()*1000+
		";acc="+activeMining.account+";sig=signature;mdt="+activeMining.tokenIndex+
		";dat:"+mym.length+"="+mym+";";
	var res = "mvl=PowMinedBlock;mss:"+mss.length+"="+mss+";";
	return(res);
}

function sha512(abuffer, continuation) {
	var p = window.crypto.subtle.digest({name: "SHA-512"}, abuffer);
	p.then(function(hash){
		continuation(new Uint8Array(hash));
	});
	p.catch(function(err){
		console.error(err);
	});
}

function jsMinerCheckBlock(bb, non) {
	// guiNotify("Javascript mining step for nonce == " + non + "\n");
	sha512(stringToArrayBuffer(bb), function(h1) {
		sha512(h1, function(blockHash) {
			var difficulty = activeMining.difficulty;
			if (difficulty == null || difficulty == undefined) return;
			for(var i=0; i<difficulty; i++) {
				if (((blockHash[Math.floor(i/8)]>>(i%8)) & 0x01) != 0) return;
			}
			guiNotify("" + new Date() + ": Info: Javascript miner got block at difficulty "+difficulty+" and nonce == "+non+"\n");
			jsvar.setString('action', 
							'action=blockmined'
							+'&token='+activeMining.token
							+'&account='+activeMining.account
							+'&block='+encodeURIComponent(bb)
						   );
			// small hack
			minerTokens[activeMining.tokenIndex].minedblock = "";
		});
	});
}

function jsMiningStep() {
	var block =  jsMiningGenerateBlockCandidate();
	if (block != null) {
		// TODO: set load level between 1..100
		for(var i=0; i<50; i++) {
			var non = activeMining.nonce = jsMinerIncrementNonce(activeMining.nonce);
			var bb = block+'non='+non+';';
			jsMinerCheckBlock(bb, non);
		}
	}
}

function jsActiveMiningClear() {
	if (activeMining != null && activeMining != undefined) {
		if (activeMining.interval != null) clearInterval(activeMining.interval);
	}	
	activeMining = null;	
	miningLedRed();
}

function startMiningPressed(form) {
	jsActiveMiningClear();
	var sel = form.elements['token'];
	var token = sel.value;
	var tokenIndex = sel.selectedIndex;
	var account = form.elements['account'].value;
	console.log("MM", form, token, account);
	if (token == null || token == "" || account == null || account == "") {
		guiNotify("" + new Date() + ": Error: Wrong token or account.\n");
		return;
	}
	var difficulty = supportedTokenTypes[tokenIndex].difficulty;
	if (difficulty == null || difficulty == undefined) {
		guiNotify("" + new Date() + ": Error: Wrong mining difficulty. Try to reload page.\n");
		return;
	}
    guiNotify("" + new Date() + ": Info: Mining allowed for "+token+" for account "+account+" and difficulty "+difficulty+"\n");
	var interval = setInterval(jsMiningStep, 10);
	activeMining = {token:token, tokenIndex:tokenIndex, account:account, difficulty:difficulty, nonce:"", interval:interval};
	miningLedGreen();
};

function stopMiningPressed(form) {
	jsActiveMiningClear();
    guiNotify("" + new Date() + ": Info: Stop mining.\n");
};

function tokenChanged(form) {
	stopMiningPressed(form);
};


