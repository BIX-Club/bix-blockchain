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

// jsvar.debuglevel = 999;

if (typeof jsvar==='undefined') {
	jsvar={variableUpdate:{}, variableArrayResize:{}};
	jsvar.connected=false;
};

jsvar.allowValueInsertion = true;

var msgsCounter = 0;

var walletCarouselLoginIndex = 0;
var walletCarouselMesagesIndex = 1;
var walletCarouselBalancesIndex = 2;
var walletCarouselSendMoneyIndex = 3;
var walletCarouselUploadFileIndex = 4;
var walletCarouselDownloadFileIndex = 5;
var walletCarouselCreateTokenIndex = 6;
var walletCarouselTokenParsIndex = 7;
var walletCarouselExchangeIndex = 8;
var walletCarouselBlockExplorer = 9;
var walletCarouselNewTokenPars = 10;
var walletCarouselNewTokenFinalInfo = 11;

var walletActiveCarouselIndex = 0;
var walletPeriodicUpdatesId = 0;

var crypto = window.crypto || window.msCrypto; 
var userfilecounterarray = new Uint8Array([21,31,131,65,1,87,53,103,33,59,23,54,11,89,74,16]);
var userfileencryptioninjavascript = false

function msgWrite(msg) {
    var ss = document.getElementById('msgspan');
	if (ss == null || ss == undefined) return;
    ss.innerText += msg;
    // scroll to bottom
    var objDiv = document.getElementById("msgsdiv");
    objDiv.scrollTop = objDiv.scrollHeight;
};
function openNewtab(url) {
	window.open(url,'_blank');
}
function escapeHtml(string) {
	var entityMap = {
		'&': '&amp;',
		'<': '&lt;',
		'>': '&gt;',
		'"': '&quot;',
		"'": '&#39;',
		'/': '&#x2F;',
		'`': '&#x60;',
		'=': '&#x3D;'
	};
	return String(string).replace(/[&<>"'`=\/]/g, function (s) {
		return entityMap[s];
	});
}
function guiAddMessage(type, header, body, prependFlag) {
	msgsCounter ++;
	var div = document.createElement('div');
	div.innerHTML = "<button class='btn btn-"+type+" w-100 text-left' type='button' data-toggle='collapse' data-target='#msg"+msgsCounter+"' aria-expanded='false' aria-controls='collapseExample'>"
		+ escapeHtml(header)
		+ "</button>"
		+ "<div class='collapse' id='msg"+msgsCounter+"'>"
		+ "<div class='card card-body'>"
		+ escapeHtml(body)
		+ "</div>"
		+ "</div>"
	;
	var msgse = document.getElementById('messagesdiv');
	if (msgse == null || msgse == undefined) return;
	if (prependFlag) {
		msgse.insertBefore(div, msgse.firstElementChild);
	} else {
		msgse.insertBefore(div, msgse.lastElementChild);
	}
}
function guiCleanNotifications() {
	var msgse = document.getElementById('messagesdiv');
	if (msgse == null || msgse == undefined) return;
	while (msgse.childElementCount > 1) {
		msgse.removeChild(msgse.firstChild);
	}
}
function guiNotificationParseMsg(msg) {
	var type = 'info';
	if (msg.substring(4,5) == "-" && msg.substring(7,8) == "-" && msg.substring(16,17) == ":") {
		var b = msg.substring(25);
	} else {
		var b = msg;
	}
	i = b.indexOf('Error');
	if (i >= 0 && i < 15) {
		type = 'danger';
		b = b.substring(b.indexOf(':')+1);
	} else {
		i = b.indexOf('Warning');
		if (i >= 0 && i < 5) {
			type = 'warning';
			b = b.substring(b.indexOf(':')+1);			
		} else {
			i = b.indexOf('Info');
			if (i >= 0 && i < 5) {
				type = 'info';
				b = b.substring(b.indexOf(':')+1);			
			}
		}
	}
	// TODO: do this more device responsive
	var h = msg.substring(0, 100);
	return({type:type, header:h, body:b});
}
function guiNotify(msg) {
	// console.log("MSG", msg);
	// msgWrite(msg);
	var pp = guiNotificationParseMsg(msg);
	guiAddMessage(pp.type, pp.header, pp.body, true);
	$.notify({
		// options
		message: pp.body
	},{
		// settings
		type: pp.type,
		placement: {align: 'left'},
		offset: {y:60},
		delay: 4000
	});
}
function guiNotifyAppend(msg) {
	var pp = guiNotificationParseMsg(msg);
	guiAddMessage(pp.type, pp.header, pp.body, false);
}
function resizeTable(tab, newDimension, ncells) {
	// resize the table to newDimension rows
	for(var j = tab.rows.length; j > newDimension; j--) {
		tab.deleteRow(-1);
	}
	while (newDimension > tab.rows.length) {
		var row = tab.insertRow(-1);
		for(var i=0; i<ncells; i++) row.insertCell(-1);
	}
};
function arrayBufferToHex(buffer) {
    var res = '';
	if (buffer == null) return(res);
    var bytes = new Uint8Array( buffer );
    var len = bytes.byteLength;
    for (var i = 0; i < len; i++) {
        res += ("0"+(Number(bytes[i]).toString(16))).slice(-2);
    }
    return(res);
};
function arrayBufferFromHex(hex) {
	if (hex == null || hex == "") return(null);
	var len = hex.length / 2;
	var buf = new ArrayBuffer(len);
	var bufView = new Uint8Array(buf);
	for(var i=0; i<len; i++) {
		buf[i] = parseInt(hex.substring(i*2, i*2+2), 16);
	}
	return(buf);
}
function arrayBufferToString(buffer) {
    var res = '';
    var bytes = new Uint8Array( buffer );
    var len = bytes.byteLength;
    for (var i = 0; i < len; i++) {
        res += String.fromCharCode(bytes[i]);
    }
    return(res);
};
function arrayBufferFromString(str) {
	var buf = new ArrayBuffer(str.length);
	var bufView = new Uint8Array(buf);
	var len = str.length;
	for (var i=0; i<len; i++) {
		bufView[i] = str.charCodeAt(i);
	}
	return buf;
};
function arrayBufferToBase64(buffer, callback ) {
    var blob = new Blob([buffer],{type:'application/octet-binary'});
    var reader = new FileReader();
    reader.onload = function(evt){
        var dataurl = evt.target.result;
        callback(dataurl.substr(dataurl.indexOf(',')+1));
    };
    reader.readAsDataURL(blob);
}
function arrayBufferFromBase64(base64Str, callback) {
	var req = new XMLHttpRequest;
	// surprisingly, Chrome's limit of 2mb for URL size does not hurt here
	req.open('GET', "data:application/octet;base64," + base64Str);
	req.responseType = 'arraybuffer';
	req.onload = function fileLoaded(e) {
		var byteArray = new Int8Array(e.target.response);
		callback(byteArray);
	}
	req.send();
}
function arrayBufferToURIComponent(buffer) {
    var res = '';
	if (buffer == null) return(res);
    var bytes = new Uint8Array( buffer );
    var len = bytes.byteLength;
    for (var i = 0; i < len; i++) {
        res += '%' + ("0"+(Number(bytes[i]).toString(16))).slice(-2);
    }
    return(res);
};
function getUrlParameter(url, name) {
    var match = RegExp('[?&]' + name + '=([^&#]*)').exec(url);
    return (match && decodeURIComponent(match[1].replace(/\+/g, ' ')));
};
function createRequestStringFromClassName(elem) {
	var res = '';
	var e = elem.querySelectorAll("input,select");
	for (var i=0; i<e.length; i++) {
		var cls = e[i].getAttribute("class");
		// always taking last class name
		var c = cls.substring(cls.lastIndexOf(" ") + 1);
		res += '&'+ c + '=' + encodeURIComponent(e[i].value);
	}
	if (res.length > 0) res = res.substr(1);
    return(res);
};
function getRequestValueString(form, name) {
	if (form.elements[name] == null || form.elements[name] == undefined) {
		var value = "";
	} else {
		var value = form.elements[name].value;
	}
	var res = name + '=' + encodeURIComponent(value);
	return(res);
}
function getKYCRequestString(form) {
	var res = "";
	
	res = getRequestValueString(form, "firstName")
		+ '&' + getRequestValueString(form, "lastName")
		+ '&' + getRequestValueString(form, "email")
		+ '&' + getRequestValueString(form, "country")
		+ '&' + getRequestValueString(form, "telephone")
		+ '&' + getRequestValueString(form, "address")
	;
	
	return(res);
}

var saveBase64EncodedFile = function saveBase64EncodedFile(filename, base64content) {
	var a = document.createElement("a");
	document.body.appendChild(a);
	(saveBase64EncodedFile = function (filename, base64content) {
		a.target='_blank';
		a.download = filename;
		if (base64content.length > 2000000) {
			// createObjectURL is not available on all browsers
			arrayBufferFromBase64(base64content, function(data) {
				var blob = new Blob([data], {type: "octet/stream"}),
				url = window.URL.createObjectURL(blob);
				a.href = url;
				a.click();
				window.URL.revokeObjectURL(url);
			});
		} else {
			// Chrome has a limit 2MB for URL size, so this works only for small files
			var url = "data:text/plain;base64,"+base64content;
			a.href = url;
			a.click();
			window.URL.revokeObjectURL(url);
		}
	}) (filename, base64content);
}

function saveFile(name, data) {
	saveBase64EncodedFile(name, btoa(data))
};

function readFileBeforeContinuation(form, fileselector, continuation) {
	var reader = new FileReader();
	var file = form.elements[fileselector].files[0];
	if (! file) {
		msgWrite("Warning: no file selected in "+fileselector+".\n");
		continuation("", 0, null);
		return;
	}
	var fname = file.name;
	var reader = new FileReader();
	reader.onload = function(e) {
		var content = reader.result;
		var len = content.byteLength;
		continuation(fname, len, content);
	};
	reader.readAsArrayBuffer(file);
	msgWrite("Reading "+fname+".\n");
}

function readFileToBase64BeforeContinuation(form, fileselector, continuation) {
	readFileBeforeContinuation(form, fileselector, function(idfname, idflen, idfcontent) {
		arrayBufferToBase64(idfcontent, function (base64Content) {
			continuation(idfname, idflen, base64Content);
		});
	});
}

function readFileEncryptAndBase64BeforeContinuation(form, fileselector, rawkey, rawcounter, continuation) {
	readFileBeforeContinuation(form, fileselector, function(idfname, idflen, idfcontent) {
		if (rawkey == null) {
			arrayBufferToBase64(idfcontent, function (base64Content) {
				continuation(idfname, idflen, base64Content);
			});
		} else {
			var pp = crypto.subtle.importKey("raw", rawkey, {name: "AES-CTR"}, true, ["encrypt", "decrypt"]);
			pp.then(function(key){
				var pp = crypto.subtle.encrypt({name: "AES-CTR", counter: rawcounter, length: 128}, key, idfcontent);
				pp.then(function(encrypted){
					arrayBufferToBase64(encrypted, function (base64Content) {
						continuation(idfname, idflen, base64Content);
					});
				}).catch(function(err){guiNotify(err.toString());});
			}).catch(function(err){guiNotify(err.toString());});
		}
	});
}

function createNewAccountPressed(form) {
    var token = form.elements['token'].value;
    msgWrite("Requesting creation of a new "+token+" account.\n");
	readFileBeforeContinuation(form, 'id-scan-file', function(idfname, idflen, idfcontent) {
		readFileBeforeContinuation(form, 'proof-of-address-file', function(podfname, podflen, podfcontent) {
			if (idflen > 2000000) {
				msgWrite("File "+idfname+" is too long. Request not processed.\n");
				return;
			}
			if (podflen > 2000000) {
				msgWrite("File "+podfname+" is too long. Request not processed.\n");
				return;
			}
			jsvar.setString('action', 
							'action=newaccount'
							+'&token='+encodeURIComponent(token)
							+'&formname='+encodeURIComponent(form.name)
							+'&' + getKYCRequestString(form)
							+'&idscanfname=' + encodeURIComponent(idfname)
							+'&idscan=' + arrayBufferToURIComponent(idfcontent)
							+'&podscanfname=' + encodeURIComponent(podfname)
							+'&podscan=' + arrayBufferToURIComponent(podfcontent)
							+'&gdprserver=' + encodeURIComponent(form.elements['gdprserver'].value)
						   );
		});
	});
}

function makeHttpRequest(request, continuation) {
    var xmlhttp;
    xmlhttp=new XMLHttpRequest();
    xmlhttp.onreadystatechange = function() {
        if (xmlhttp.readyState==4 && xmlhttp.status==200) {
			// console.log("GOT", xmlhttp.responseType, xmlhttp.response);
            continuation(xmlhttp.response);
        }
    };
    xmlhttp.open("GET", request, true);
	xmlhttp.responseType = 'blob';
    xmlhttp.send();
}

function onNewAccountCreated(account, pkey, formname) {
	document.forms[formname].elements['newaccount'].value = account;
	document.forms[formname].elements['newprivatekey'].value = pkey;
    var image = document.getElementById('newprivateKeyImg');
	// console.log("reading", image.src);
	// TODO: remove account number from file name
	var fname = 'private-key-' + account + '.jpg';
	makeHttpRequest('key.jpg', function(blob) {
		var reader = new FileReader();
		reader.readAsDataURL(blob); 
		reader.onloadend = function() {
			base64 = reader.result;
			var zeros = {};
			var exif = {};
			exif[piexif.ExifIFD.UserComment] = pkey;
			var exifObj = {"0th":zeros, "Exif":exif, "GPS":{}};
			var exifbytes = piexif.dump(exifObj);
			// console.log("Adding", exifbytes, base64);
			var inserted = base64;
			var inserted = piexif.insert(exifbytes, base64);
			// console.log("Result URL", inserted);
			image.src = inserted;
			image.name = fname;
			image.title = fname;
			image.download = fname;
			image.filename = fname;
			/*
			var hh = document.getElementById('newprivateKeyHref');
			hh.download = fname;
			hh.href = inserted;
			*/
		}
	});

	// also go to saving page
	navigationAndCarouselSwitchTo(1);
}

function savePrivateKey(form, prefix) {
    // msgWrite("Saving private key");
    var privatekey = form.elements[prefix+'privatekey'].value;
	// TODO: remove account number from file name
	saveFile("privateKey-" + form.elements[prefix+'account'].value + ".txt", privatekey);
}

function loadPrivateKey(form) {
	var destElem = form.elements['privatekey'];
	var fileselectorid = 'pkfileselector';
	readFileBeforeContinuation(form, fileselectorid, function(fname, flen, fcontent) {
		var ext = fname.split('.').pop().toLowerCase();
		if (ext == "txt") {
			// pure text file containing private key
			msgWrite("Read as text file\n");
			var key = arrayBufferToString(fcontent);			
			destElem.value = key;
		} else if (ext == "jpg" || ext == "jpeg" || ext == "png") {
			// get the key from an image
			msgWrite("Read as image\n");
			EXIF.getData(form.elements[fileselectorid].files[0], function() {
				var key = EXIF.getTag(this, "UserComment");
				destElem.value = key;
			});
		} else {
			// unknown file format, try to extract private key anyway
			msgWrite("Read as unknown format file\n");
			var key = arrayBufferToString(fcontent);
			var beginmarker = "-----BEGIN PRIVATE KEY-----";
			var endmarker = "-----END PRIVATE KEY-----";
			var bi = key.indexOf(beginmarker);
			var ei =  key.lastIndexOf(endmarker);
			if (bi >= 0 && ei >= 0) {
				key = key.substring(bi, ei) + endmarker;
			}
			destElem.value = key;
		}
	});
}

function onGotoWalletPressed(form) {
	openNewtab('account.html');
}

function loginPressed(form) {
    var token = form.elements['token'].value;
    var account = form.elements['account'].value;
    var privatekey = form.elements['privatekey'].value;
    jsvar.setString('action', 
                    'action=login'
                    +'&token='+encodeURIComponent(token)
                    +'&account='+encodeURIComponent(account)
                    +'&privatekey='+encodeURIComponent(privatekey)
				   );
	guiCleanNotifications();
}

function loginNotification(msg, account) {
	guiNotify(msg);
	if (msg.search(/error/i) == -1) {
		navigationAndCarouselSwitchTo(walletCarouselBalancesIndex); 
		$('.loginnamespan').html(account);
		$('input.base_account').val(account);
		var form = document.getElementById("accountform");
		getAllBalancesPressed(form);
		getBalancePressed(form);
	}
}
function onTokenTransferTokenChanged(form) {
	var token = form.elements['token'].value;
	$('.tokentransfertoken').html(token);
	for(i=0; i<supportedTokenTypes.length; i++) {
		if (supportedTokenTypes[i].name == token) {
			$('.tokentransferfee').html('' + supportedTokenTypes[i].transferfee);
			break;
		}
	}
}
function bytetokenUploadFilePressed(form) {
    var account = form.elements['account'].value;
    var privatekey = form.elements['privatekey'].value;

	if (userfileencryptioninjavascript) {
		readFileEncryptAndBase64BeforeContinuation(
			form, 
			'fileselector', 
			arrayBufferFromHex(form.elements['uploadskey'].value), 
			userfilecounterarray,
			function(fname, flen, fcontent) {
				msgWrite("Requesting insertion of file "+fname+" ("+flen+" bytes) into the blockchain.\n");
				if (flen > 8000000) {
					msgWrite("File is too long.\n");
					return;
				}
				jsvar.setString('action', 
								'action=upload'
								+'&account='+encodeURIComponent(account)
								+'&privatekey='+encodeURIComponent(privatekey)
								+'&filename='+encodeURIComponent(fname)
								+'&file='+encodeURIComponent(fcontent)
								+'&formname='+form.name
							   );
			});
	} else {
		readFileToBase64BeforeContinuation(
			form, 
			'fileselector', 
			function(fname, flen, fcontent) {
				msgWrite("Requesting insertion of file "+fname+" ("+flen+" bytes) into the blockchain.\n");
				if (flen > 8000000) {
					msgWrite("File is too long.\n");
					return;
				}
				jsvar.setString('action', 
								'action=upload'
								+'&account='+encodeURIComponent(account)
								+'&privatekey='+encodeURIComponent(privatekey)
								+'&filename='+encodeURIComponent(fname)
								+'&file='+encodeURIComponent(fcontent)
								+'&uploadskey='+encodeURIComponent(form.elements['uploadskey'].value), 
								+'&formname='+form.name
							   );
			});
	}
}

function saveDownloadedBase64FileContent(fname, fcontent) {
	// console.log("SAVE FILE", fname, fcontent.length, fcontent.substring(0,100), "...", fcontent.substring(fcontent.length-100));
	var skey = null;
	var form = document.getElementById("accountform");
	if (form != null && form != undefined) {
		skey = form.elements['downloadskey'].value;
	}
	if (userfileencryptioninjavascript && skey != null && skey != "") {
		var bkey = arrayBufferFromHex(skey);
		arrayBufferFromBase64(fcontent, function (bytecontent) {
			var pp = crypto.subtle.importKey("raw", bkey, {name: "AES-CTR"}, true, ["encrypt", "decrypt"]);
			pp.then(function(key){
				var pp = crypto.subtle.decrypt({name: "AES-CTR", counter: userfilecounterarray, length: 128}, key, bytecontent);
				pp.then(function(decrypted){
					arrayBufferToBase64(decrypted, function (dcontent) {
						saveBase64EncodedFile(fname, dcontent);
					});
				}).catch(function(err){guiNotify(err.toString());});
			}).catch(function(err){guiNotify(err.toString());});
		});
	} else {
		saveBase64EncodedFile(fname, fcontent);
	}
}
function bytetokenDownloadFilePressed(form) {
	if (userfileencryptioninjavascript) {
		jsvar.setString('action', 
						'action=download'
						+'&blockid='+encodeURIComponent(form.elements['blockid'].value)
						+'&msgid='+encodeURIComponent(form.elements['msgid'].value)
						+'&formname='+form.name
					   );
	} else {
		jsvar.setString('action', 
						'action=download'
						+'&blockid='+encodeURIComponent(form.elements['blockid'].value)
						+'&msgid='+encodeURIComponent(form.elements['msgid'].value)
						+'&downloadskey='+encodeURIComponent(form.elements['downloadskey'].value)
						+'&formname='+form.name
					   );
	}
}
function bytetokenGenerateNewSymmetricKey(form) {
	var pp = crypto.subtle.generateKey({name: "AES-CTR", length: 256}, true, ["encrypt", "decrypt"]);
	pp.then(function(key){
		var pp = crypto.subtle.exportKey("raw", key);
		pp.then(function(keydata){
			form.elements['uploadskey'].value = arrayBufferToHex(keydata);
		}).catch(function(err){guiNotify(err.toString());});
	}).catch(function(err){guiNotify(err.toString());});
};
function bytetokenLoadUploadSkey(form) {
	readFileBeforeContinuation(form, 'uppassfileselector', function(fname, flen, fcontent) {
		form.elements['uploadskey'].value = arrayBufferToString(fcontent);
	});
};
function bytetokenLoadDlSkey(form) {
	readFileBeforeContinuation(form, 'dlpassfileselector', function(fname, flen, fcontent) {
		form.elements['downloadskey'].value = arrayBufferToString(fcontent);
	});
};
function bytetokenSaveSkey(form) {
	var rr = form.elements['uploadskey'].value;
	saveFile("symmetricKey.txt", rr);
};
function onTokenTypeChanged(form, prefix) {
	var tokentype = form.elements[prefix+'tokentype'].value;
	if (tokentype == null || tokentype == undefined) return;
	$('.'+prefix+'tokenoptions').addClass('d-none');
	$('.'+prefix+'tokenoptions.' + tokentype).removeClass('d-none');
}
function onExchangeTokenChanged(form) {
	exchangeUpdateOrderbookPressed(form);
}
function onExchangeOrderTypeChanged(form) {
	var orderType = form.elements['exchangeOrderType'].value;
	$('.exchangeExecutionOption').hide();
	$('.exchangeExecutionOption'+orderType).show();
}
function updateNewTokenLinks(form) {
	if (form == null) return;
	var ee = form.elements['newtokenname'];
	if (ee == null) return;
    var token = ee.value;
	form.elements['tokenname'].value = token;
}
function onCreateNewCurrencyPressed(form) {
    var account = form.elements['account'].value;
    var privatekey = form.elements['privatekey'].value;
    var token = form.elements['newtokenname'].value;
    var tokentype = form.elements['newtokentype'].value;
    if (token == null || token == "") return;
    // msgWrite("Requesting creation of a new token "+token+" of type "+tokentype+".\n");
    jsvar.setString('action', 
                    'action=newtoken'
                    +'&token='+encodeURIComponent(token)
                    +'&tokentype='+encodeURIComponent(tokentype)
					+'&formname='+form.name
                    +'&account='+encodeURIComponent(account)
                    +'&privatekey='+encodeURIComponent(privatekey)
				   );
	// TODO: There must be some mechanins of confirmation/continuation of requests
	// document.getElementById('createaccountref').href = 'index.html?token='+token; 
	updateNewTokenLinks(form);
};
function onGotoNewTokenParameters(formname) {
	var form = document.forms[formname];
	// var baseacc = form.elements['newaccount'].value;
	var baseacc = form.elements['account'].value;
	// console.log("b", baseacc);
	var tt = document.querySelectorAll('.newtokenoptions .base_account');
	// console.log("c",tt);
	for(var i=0; i<tt.length; i++) tt[i].value = baseacc;
	$('#walletCarousel').carousel(walletCarouselNewTokenPars); 
}
function onTokenInitParametersPressed(form) {
    var account = form.elements['account'].value;
    var privatekey = form.elements['privatekey'].value;
    var token = form.elements['newtokenname'].value;
    var tokentype = form.elements['newtokentype'].value;
    if (token == null || token == "") return;
    // msgWrite("Setting options for token "+token+" of type "+tokentype+".\n");
	var options = createRequestStringFromClassName(document.querySelector('.newtokenoptions.' + tokentype));
    jsvar.setString('action', 
                    'action=tokenoptions'
                    +'&token='+encodeURIComponent(token)
                    +'&tokentype='+encodeURIComponent(tokentype)
					+'&opt='+encodeURIComponent(options)
					+'&formname='+form.name
                    +'&account='+encodeURIComponent(account)
                    +'&privatekey='+encodeURIComponent(privatekey)
				   );
	$('#walletCarousel').carousel(walletCarouselNewTokenFinalInfo); 
}
function onTokenChangeOptions(form) {
    var account = form.elements['account'].value;
    var privatekey = form.elements['privatekey'].value;
    var token = form.elements['tokenname'].value;
    var tokentype = form.elements['tokentype'].value;
    if (token == null || token == "") return;
    msgWrite("Setting options for token "+token+" of type "+tokentype+".\n");
	var options = createRequestStringFromClassName(document.querySelector('.tokenoptions.' + tokentype));
    jsvar.setString('action', 
                    'action=tokenoptions'
                    +'&token='+encodeURIComponent(token)
                    +'&tokentype='+encodeURIComponent(tokentype)
					+'&opt='+encodeURIComponent(options)
					+'&formname='+form.name
                    +'&account='+encodeURIComponent(account)
                    +'&privatekey='+encodeURIComponent(privatekey)
				   );
};

function updateTokenListPressed(form) {
    jsvar.setString('action', 
                    'action=updatetokenlist'
					+'&formname='+form.name
				   );
};
function getBalancePressed(form) {
    //console.log("form", form.toString());
    var token = form.elements['token'].value;
    var account = form.elements['account'].value;
    var privatekey = form.elements['privatekey'].value;
    msgWrite("Requesting balance of "+token+" account "+account+".\n");
    jsvar.setString('action', 
                    'action=getbalance'
                    +'&token='+encodeURIComponent(token)
                    +'&account='+encodeURIComponent(account)
                    +'&privatekey='+encodeURIComponent(privatekey)
					+'&formname='+form.name
				   );
};
function getAllBalancesPressed(form) {
    //console.log("form", form.toString());
    var token = form.elements['token'].value;
    var account = form.elements['account'].value;
    var privatekey = form.elements['privatekey'].value;
    msgWrite("Requesting balance of "+token+" account "+account+".\n");
    jsvar.setString('action', 
                    'action=getallbalances'
                    +'&account='+encodeURIComponent(account)
                    +'&privatekey='+encodeURIComponent(privatekey)
					+'&formname='+form.name
				   );
};
function sendTokensPressed(form) {
    //console.log("form", form.toString());
    var token = form.elements['token'].value;
    var amount = form.elements['amount'].value;
    var fromaccount = form.elements['account'].value;
    var toaccount = form.elements['toaccount'].value;
    var privatekey = form.elements['privatekey'].value;
    msgWrite("Requesting send of "+amount+" "+token+" from "+fromaccount+" to "+toaccount+".\n");
    jsvar.setString('action', 
                    'action=send'
                    +'&token='+token
                    +'&amount='+encodeURIComponent(amount)
                    +'&fromaccount='+encodeURIComponent(fromaccount)
                    +'&toaccount='+encodeURIComponent(toaccount)
                    +'&privatekey='+encodeURIComponent(privatekey)
					+'&formname='+form.name
				   );
};
function exchangeUpdateOrderbookPressed(form) {
    var account = form.elements['account'].value;
	var baseToken = form.elements['exchangeOrderBaseToken'].value;
	var quoteToken = form.elements['exchangeOrderQuoteToken'].value;
    var privatekey = form.elements['privatekey'].value;
    jsvar.setString('action', 
                    'action=exchangeUpdateOrderbook'
                    +'&account='+encodeURIComponent(account)
                    +'&baseToken='+encodeURIComponent(baseToken)
                    +'&quoteToken='+encodeURIComponent(quoteToken)
                    +'&privatekey='+encodeURIComponent(privatekey)
					+'&formname='+form.name
				   );	
}
function exchangeNewOrderPressed(form) {
    var account = form.elements['account'].value;
	var baseToken = form.elements['exchangeOrderBaseToken'].value;
	var quoteToken = form.elements['exchangeOrderQuoteToken'].value;
	var quantity = form.elements['exchangeQuantity'].value;
	var side = form.elements['exchangeOrderSide'].value;
	var orderType = form.elements['exchangeOrderType'].value;
	var limitPrice = form.elements['exchangeOrderLimitPrice'].value;
	var stopPrice = form.elements['exchangeOrderStopPrice'].value;
    var privatekey = form.elements['privatekey'].value;
    jsvar.setString('action', 
                    'action=exchangeNewOrder'
                    +'&account='+encodeURIComponent(account)
                    +'&baseToken='+encodeURIComponent(baseToken)
                    +'&quoteToken='+encodeURIComponent(quoteToken)
                    +'&quantity='+encodeURIComponent(quantity)
                    +'&side='+encodeURIComponent(side)
                    +'&orderType='+encodeURIComponent(orderType)
                    +'&limitPrice='+encodeURIComponent(limitPrice)
                    +'&stopPrice='+encodeURIComponent(stopPrice)
                    +'&privatekey='+encodeURIComponent(privatekey)
					+'&formname='+form.name
				   );	
}
jsvar.variableUpdate["supportedTokenTypes~"] = function (i, v) {
	// update token selections
	$(".tokenselect").each(function (i) {
		var ss = $(this);
		var sel = ss.val();
		ss.empty();
		for(var i=0, j=0; i<supportedTokenTypes.length; i++) {
			var name = supportedTokenTypes[i].name;
			ss.append($("<option></option>").attr("value", name).text(name));
		}
		if (sel != null && sel != undefined && sel != "") {
			ss.val(sel);
		}
	});
};


jsvar.variableUpdate["coreServers~"] = function (i, v) {
	var ss = $("select[name=gdprserver]");
	var sel = ss.val();
    ss.empty();
    for(var i=0; i<coreServers.length; i++) {
		var name = coreServers[i].gdpr;
		if (name != null && name != undefined && name != "") {
			ss.append($("<option></option>").attr("value", i).text(name));
			if (sel === null || sel === undefined || sel === "") {
				sel = i;
			}
		}
    }
	if (sel !== null && sel !== undefined && sel !== "") {
		ss.val(sel);
	}
};


////////////////////////
// pending requests stuff

function pendingRequestsTableResize(newDimension) {
	var tab = document.getElementById('requestsTable');
	if (tab == null) return;
	resizeTable(tab, newDimension, 2);
}

function pendingRequestsTableSetItem(i, val, reqType) {
	var tab = document.getElementById('requestsTable');
	if (tab == null) return;
	var e = tab.rows[i];
	e.cells[0].innerHTML = val;
	e.cells[1].innerHTML = "<button class=buttonLink onclick='onExplorePendingRequestPressed(this.form, &quot;"+val+"&quot;, &quot;"+reqType+"&quot;);'>Explore</button>";
	if (reqType == "pendingRequest") {
		e.cells[1].innerHTML += "<button class=buttonLink onclick='onApprovePendingRequestPressed(this.form, &quot;"+val+"&quot;, &quot;"+reqType+"&quot;);'>Approve</button>";
		e.cells[1].innerHTML += "<button class=buttonLink onclick='onDeletePendingRequestPressed(this.form, &quot;"+val+"&quot;, &quot;"+reqType+"&quot;);'>Delete</button>";
	} else {
		// e.cells[2].innerHTML = "";
		// e.cells[3].innerHTML = "";
	}
}
function accountBalancesInit() {
	accountBalances = [];
}
function accountBalancesAddBalance(token, value) {
	accountBalances[token] = value;
}
function accountBalancesDone() {
	var tab = document.getElementById('balancesTable');
	// go through supportedTokenTypes to get tokens ordered aphabetically
	resizeTable(tab, 1, 2);
	for(var i=0,j=0; i<supportedTokenTypes.length; i++) {
		var token = supportedTokenTypes[i].name;
		if (token != null && token != undefined && token != "") {
			var balance = accountBalances[token];
			if (balance != null && balance != undefined && balance != 0) {
				j++;
				resizeTable(tab, j+1, 2);
				tab.rows[j].setAttribute('scope', 'row');
				tab.rows[j].cells[0].setAttribute('scope', 'col');
				//tab.rows[j].cells[0].classList.add("text-right");
				tab.rows[j].cells[0].innerHTML = '' + token;
				tab.rows[j].cells[1].setAttribute('scope', 'col');
				//tab.rows[j].cells[1].classList.add("text-left");
				tab.rows[j].cells[1].innerHTML = balance.toLocaleString();
			}
		}
	}
}
function updateBookTable(tabId, a, cellprice, cellqty, priceAlignment) {
	var tab = document.getElementById(tabId);
	if (a == null || a == undefined) {
		resizeTable(tab, 1, 2);
		return;
	}
	resizeTable(tab, a.length, 2);
	for(var i=1; i<a.length; i++) {
		tab.rows[i].cells[cellprice].innerHTML = '' + a[i-1][0];
		tab.rows[i].cells[cellprice].classList.add(priceAlignment);
		tab.rows[i].cells[cellqty].innerHTML = '' + a[i-1][1];
	}
}
function orderbookUpdate(baseToken, quoteToken, bids, asks) {
	// console.log("orderbookUpdate", dummy, bids, asks);
	document.getElementById("orderbookSpan").innerHTML = baseToken + ' / ' + quoteToken;
	updateBookTable("bidsTable", bids, 1, 0, "text-right");
	updateBookTable("asksTable", asks, 0, 1, "text-left");
}
jsvar.variableArrayResize["pendingRequests"] = function (newDimension) {
	console.log("pendingRequests: Resizing to", newDimension);
	for(var i=pendingRequests.length; i<newDimension; i++) pendingRequests[i] = {};
	pendingRequests.length = newDimension;
	pendingRequestsTableResize(newDimension);
};

jsvar.variableUpdate["pendingRequests[?].name"] = function (i, val) {
	console.log("pendingRequests: Setting ", i, val);
	pendingRequests[i] = val;
	pendingRequestsTableSetItem(i, val, "pendingRequest");
}

function formParametersAppendix(form) {
	if (form == null) return('');

	var token = form.elements['token'].value;
	var account = form.elements['account'].value;
	var privatekey = form.elements['privatekey'].value;
	res = ''
		+'&token='+encodeURIComponent(token)
        +'&account='+encodeURIComponent(account)
        +'&privatekey='+encodeURIComponent(privatekey)
		+'&formname='+form.name
	;
	return(res);
}

function onExplorePendingRequestPressed(form, request, requestType) {
	// console.log("EXPLORE", form, request);
	jsvar.setString('action', 
					'action=pendingRequestExplore'
					+'&request='+encodeURIComponent(request)
					+'&requestType='+encodeURIComponent(requestType)
					+ formParametersAppendix(form)
				   );
}

function onApprovePendingRequestPressed(form, request, requestType) {
	// console.log("APPROVE", form, request);
	jsvar.setString('action', 
					'action=pendingRequestApprove'
					+'&request='+encodeURIComponent(request)
					+'&requestType='+encodeURIComponent(requestType)
					+ formParametersAppendix(form)
				   );
}

function onDeletePendingRequestPressed(form, request, requestType) {
	// console.log("DELETE", form, request);
	jsvar.setString('action', 
					'action=pendingRequestDelete'
					+'&request='+encodeURIComponent(request)
					+'&requestType='+encodeURIComponent(requestType)
					+ formParametersAppendix(form)
				   );
}

function onWalletUpdatePendingRequestsTable(form) {
	// console.log("UPDATE", form);
	jsvar.setString('action', 
					'action=pendingRequestUpdate'
					+ formParametersAppendix(form)
				   );
}

function onWalletUpdateAccountList(form) {
	// console.log("UPDATE", form);
	jsvar.setString('action', 
					'action=listAccounts'
					+ formParametersAppendix(form)
				   );
}

////////////////////////////////////////////////////////////////////////////////////////////////
// explorer gui

function onExploreBlockPressed(form) {
	var blockseq = form.elements['blockseq'].value;
	// msgWrite("Exploring block "+blockseq+".\n");
	jsvar.setString('action', 
					'action=exploreblock'
					+'&blockseq='+encodeURIComponent(blockseq)
					+'&formname='+form.name
				   );
	
};
function onExploreMessagePressed(msgid) {
	jsvar.setString('action', 
					'action=exploremessage'
					+'&msgid='+encodeURIComponent(msgid)
				   );
	
};
function onSaveMsgContent() {
	saveBase64EncodedFile(messageContentFilename, messageContentBase64);
}

////////////////////////////////////////////////////////////////////////////////////////////////

jsvar.variableUpdate["pilotVersionFlag"] = function (i, val) {
	pilotVersionFlag = val;
	if (pilotVersionFlag) {
		$('.pilotversiondisabled').prop('disabled', true);
		$('.pilotversionspan').show();
		$('.productionversionspan').hide();
	} else {
		$('.pilotversiondisabled').prop('disabled', false);
		$('.pilotversionspan').hide();
		$('.productionversionspan').show();
	}
}

jsvar.variableUpdate["developmentVersionFlag"] = function (i, val) {
	developmentVersionFlag = val;
	if (developmentVersionFlag) {
		$('.developmentversiononly').show();
	} else {
		$('.developmentversiononly').hide();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

function navigationBarSwitchTo(elem) {
	$(".navbar-nav li").removeClass("active");
	$(elem).addClass("active");
}

function navigationAndCarouselSwitchTo(number) {
	$('#walletCarousel').carousel(number); 
	var nb = document.getElementById('mainNavBar');
	// console.log("NB", nb);
	if (nb == null || nb == undefined) return;
	var ne = nb.childNodes[number*2+1];
	// console.log("NE", ne);
	if (ne == null || ne == undefined) return;
	navigationBarSwitchTo(ne);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Periodic updates

function doPeriodicUpdates() {
	var form = document.getElementById("accountform");
	if (walletActiveCarouselIndex == walletCarouselBalancesIndex) {
		getAllBalancesPressed(form);
	} else if (walletActiveCarouselIndex == walletCarouselSendMoneyIndex) {
		getBalancePressed(form);
	} else if (walletActiveCarouselIndex == walletCarouselExchangeIndex) {
		exchangeUpdateOrderbookPressed(form);		
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Initialize predefined stuff from Get URL request

var previousonload = window.onload;
window.onload = function (evt) {
	if (previousonload) previousonload(evt);
	// bootsrap navigation bar switching
	$(".navbar-nav li").on("click", function() {
		navigationBarSwitchTo(this);
		$(".navbar-collapse").collapse('hide');
	});
	// bootstrap on carousel final touch
	$('#walletCarousel').on('slid.bs.carousel', function (e) {
		walletActiveCarouselIndex = e.to;
		if (e.to == walletCarouselBalancesIndex) {
			doPeriodicUpdates();
		} else if (e.to == walletCarouselSendMoneyIndex) {
			doPeriodicUpdates();
		} else if (e.to == walletCarouselTokenParsIndex) {
			// token parameters
			var form = document.getElementById("accountform");
			if (form == null || form == undefined) return;
			onTokenTypeChanged(form, '');
		} else if (e.to == walletCarouselNewTokenPars) {
			// new token parameters
			var form = document.getElementById("accountform");
			if (form == null || form == undefined) return;
			onTokenTypeChanged(form, 'new');
		} else if (e.to == walletCarouselNewTokenFinalInfo) {
			// new coin created
			var form = document.getElementById("accountform");
			updateNewTokenLinks(form);
		}
	})
	// also set some periodic updates
	if (walletPeriodicUpdatesId != 0) clearInterval(walletPeriodicUpdatesId);
	walletPeriodicUpdatesId = setInterval(doPeriodicUpdates, 10000);
}

// jsvar.onMessageReceived = function (msg) {console.log("GOT", msg.length, msg.substring(0,500), "...", msg.substring(msg.length-500));}
