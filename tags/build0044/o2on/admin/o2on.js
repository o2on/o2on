// ---------------------------------------------------------------------------
//	global
// ---------------------------------------------------------------------------
var rstep = 30;
var g_data;
var timerid = 0;

if (navigator.userAgent.indexOf("MSIE") == -1) {
	(function(){
		var events = ["mousedown", "mouseover", "mouseout", "mousemove",
									"mousedrag", "click", "dblclick"];	
		for (var i = 0; i < events.length; i++) {
			window.addEventListener(events[i], function(e) {
				window.event = e;
			}, true);
		}
	}());
};

// ---------------------------------------------------------------------------
//	onLoadFunc
//	cookieに保存されているメニューに移動
// ---------------------------------------------------------------------------
function onLoadFunc()
{
	if (document.cookie) {
		var cookies = document.cookie.split(/[;=]/);
		for (i = 0; i < cookies.length; i++) {
			if (cookies[i] == "selmenu" && cookies[i+1] != "") {
				makeContents(cookies[i+1]);
				return;
			}
		}
	}
	makeContents("query");
}

// ---------------------------------------------------------------------------
//	makeContents
//	mainフレームのxmldivにxsl+xsltのコンテンツを表示
// ---------------------------------------------------------------------------
function makeContents(menuid)
{
	showLoadingImage();
	if (navigator.userAgent.indexOf("Opera") != -1)
		makeContents2(menuid); //Opera
	else
		setTimeout("makeContents2('"+menuid+"')", 0); //IE,Firefox
}
function makeContents2(menuid)
{
	while (!top.work.document) {
		Sleep(1);
	}
	while (!top.work.document.f) {
		Sleep(1);
	}

	// MenuIDをworkフレームに保存
	with (top.work.document.f) {
		if (selmenu.value != menuid) {
			sortkey.value = "dummy";
			sortorder.value = "ascending";
			sorttype.value = "text";
		}
		selmenu.value = menuid;
		document.cookie = "selmenu=" + menuid + ";";
	}

	// メニューをハイライト
	var elm = top.menu.document.getElementById(menuid);
	elm.className = "menu3";

	// menuフレームのサイズ
	if (menuid == "controlpanel")
		parent.document.getElementsByTagName("frameset")[0].rows = "47,*,0";
	else
		parent.document.getElementsByTagName("frameset")[0].rows = "20,*,0";

	// bbsmenuフレームを表示/隠す
	if (menuid == "thread")
		parent.document.getElementsByTagName("frameset")[1].cols = "118,*";
	else
		parent.document.getElementsByTagName("frameset")[1].cols = "0,*";

	// xmlとxslのURL
	var xmlurl = "/xml/" + menuid;
	var xslurl = "/" + menuid + ".xsl";
	if (menuid == "controlpanel") {
		xmlurl = "/xml/" + top.work.document.f.controlpanelxml.value;
		xslurl = "/" + top.work.document.f.controlpanelxsl.value + ".xsl";
	}
	if (menuid == "thread")
		xmlurl += "?domain=" + top.work.document.f.domain.value + "&bbsname=" + top.work.document.f.bbsname.value;
	else {
		top.work.document.f.domain.value = "";
		top.work.document.f.bbsname.value = "";
	}

	// xml+xslをxmldivに表示
	with (top.work.document.f) {
		var lim;
		if (limit.value) lim = limit.value;
		else lim = 999999;
		xslurl += '?LIMIT=' + lim;
		xslurl += '&SORTKEY=' + sortkey.value;
		xslurl += '&SORTORDER=' + sortorder.value;
		xslurl += '&SORTTYPE=' + sorttype.value;
	}
	attachXSLT(top.main.document.getElementById("xmldiv"), xmlurl, xslurl);

	// 後処理
	if (menuid == "im")
		top.main.document.broadcast.msg.focus();
	if (top.main.document.h && top.main.document.h.lim)
		top.main.document.h.lim.value = top.work.document.f.limit.value;

	hideLoadingImage();
}

// ---------------------------------------------------------------------------
//	makeNotification
//	workフレームのxmldivにnotification.xsl+xsltのコンテンツを表示
// ---------------------------------------------------------------------------
function makeNotification()
{
	attachXSLT(top.work.document.getElementById("xmldiv"), "/xml/notification", "/notification.xsl");
	setTimeout("makeNotification()", 60000);
}

// ---------------------------------------------------------------------------
//	attachXSLT
//	指定されたノードにxsl+xsltのコンテンツを表示
// ---------------------------------------------------------------------------
function attachXSLT(node, xmlurl, xslurl)
{
	if (document.all && navigator.userAgent.indexOf("Opera") == -1) {
		xml = new ActiveXObject("Microsoft.XMLDOM");
		xsl = new ActiveXObject("Microsoft.XMLDOM");
	}
	else {
		xml = document.implementation.createDocument("", "", null);
		xsl = document.implementation.createDocument("", "", null);
	}
	xml.async = false;
	xsl.async = false;
	xml.load(xmlurl);
	xsl.load(xslurl);

	if (document.all && navigator.userAgent.indexOf("Opera") == -1) {
		node.innerHTML = xml.transformNode(xsl);
	}
	else {
		var xsltp = new XSLTProcessor();
		xsltp.importStylesheet(xsl);
		var doc = xsltp.transformToFragment(xml, window.document);
		node.innerHTML = "";
		node.appendChild(doc);
	}
}

// ---------------------------------------------------------------------------
//	openControlPanel
//	
// ---------------------------------------------------------------------------
function openControlPanelItem(xml, xsl)
{
	top.work.document.f.controlpanelxml.value = xml;
	top.work.document.f.controlpanelxsl.value = xsl;
	makeContents("controlpanel");
}

// ---------------------------------------------------------------------------
//	Sort
//	ソートキーをworkフレームに保存してコンテンツを再表示
// ---------------------------------------------------------------------------
function Sort(key, type)
{
	with (top.work.document.f) {
		if (sortkey.value == key) {
			if (sortorder.value == "ascending") sortorder.value = "descending";
			else sortorder.value = "ascending";
		}
		else
			sortorder.value = "descending";
		sortkey.value = key;
		sorttype.value = type;
	}
	makeContents(top.work.document.f.selmenu.value);
}

// ---------------------------------------------------------------------------
//	フォーム用関数
// ---------------------------------------------------------------------------
function Reload()
{
	makeContents(top.work.document.f.selmenu.value);
}
function SetLimit()
{
	top.work.document.f.limit.value = document.h.lim.value;
	makeContents(top.work.document.f.selmenu.value);
}

// ---------------------------------------------------------------------------
//	IM用
// ---------------------------------------------------------------------------
function showMsgForm(id, e_ip, port, name, pubkey)
{
	showMsgForm2(
		50/*event.clientX + document.body.scrollLeft*/,
		event.clientY + document.body.scrollTop,
		id, e_ip, port, name, pubkey);
}
function showMsgForm2(x, y, id, e_ip, port, name, pubkey)
{
	elm = document.getElementById("msgform");
	elm.style.display = "block";
	elm.style.left = x;
	elm.style.top = y;

	elm = document.getElementById("nodename");
	elm.innerHTML = name;

	document.msgform.id.value = id;
	document.msgform.e_ip.value = e_ip;
	document.msgform.port.value = port;
	document.msgform.name.value = name;
	document.msgform.pubkey.value = pubkey;
	document.msgform.msg.value = "";
	document.msgform.msg.focus();
}
function hideMsgForm()
{
	document.getElementById("msgform").style.display = "none";
}
function sendMessage()
{
	with (document.msgform) {
		if (msg.value == "") {
			alert("空のメッセージは送信できません");
			return false;
		}
		if (msg.value.indexOf("]]>") != -1) {
			alert("文字列「]]>」を含むメッセージは送信できません");
			return false;
		}
		hideMsgForm();
	}
	return true;
}
function showProfile()
{
	with (document.msgform) {
		var url = "/xml/rprofile?id=" + id.value
				+ "&e_ip=" + e_ip.value
				+ "&port=" + port.value
				+ "&name=" + name.value
				+ "&pubkey=" + pubkey.value;
		window.open(url, "_blank");
	}
	hideMsgForm();
}
function addFriend()
{
	document.msgform.action = "/xml/friend";
	document.msgform.act.value = "add";
	document.msgform.submit();
}

// ---------------------------------------------------------------------------
//	初期ノード用
// ---------------------------------------------------------------------------
function showIniNodeForm()
{
	var elm = document.getElementById("ininodeform");
	elm.style.display = "block";
	elm.style.left = document.body.clientWidth + document.body.scrollLeft - elm.offsetWidth;
	elm.style.top = event.clientY + document.body.scrollTop;
	document.ininodeform.initext.value = "";
	document.ininodeform.initext.focus();
}
function hideIniNodeForm()
{
	var elm = document.getElementById("ininodeform");
	elm.style.display = "none";
}
function sendIniNode()
{
	with (document.ininodeform) {
		if (initext.value == "") {
			alert("入力されていません");
			return false;
		}
		hideIniNodeForm();
	}
	return true;
}

// ---------------------------------------------------------------------------
//	URLから追加
// ---------------------------------------------------------------------------
function showURLIniNodeForm()
{
	var elm = document.getElementById("urlininodeform");
	elm.style.display = "block";
	elm.style.left = document.body.clientWidth + document.body.scrollLeft - elm.offsetWidth;
	elm.style.top = event.clientY + document.body.scrollTop;
	document.urlininodeform.url.focus();
}
function hideURLIniNodeForm()
{
	var elm = document.getElementById("urlininodeform");
	elm.style.display = "none";
}
function sendURLIniNode()
{
	with (document.urlininodeform) {
		if (url.value == "") {
			alert("URLを入力してください");
			return false;
		}
		hideURLIniNodeForm();
	}
	return true;
}

// ---------------------------------------------------------------------------
//	自ノード情報表示用
// ---------------------------------------------------------------------------
function showEncProfForm()
{
	var url = "/xml/ininode"
	var parser = new JKL.ParseXML(url);
	parser.onerror(parseErrorHandler);
	var data = parser.parse();
	var e = data.e;

	elm = document.getElementById("encprofform");
	elm.style.display = "block";
	elm.style.left = document.body.clientWidth + document.body.scrollLeft - elm.offsetWidth;
	elm.style.top = event.clientY + document.body.scrollTop;
	with (document.encprofform) {
		enctext.value = e;
		enctext.focus();
	}
}
function hideEncProfForm()
{
	document.getElementById("encprofform").style.display = "none";
}

// ---------------------------------------------------------------------------
//	クエリ用
// ---------------------------------------------------------------------------
function deleteQuery(hash)
{
	document.d.act.value = "delete";
	document.d.hash.value = hash;
	document.d.submit();
}
function activateQuery(hash)
{
	document.d.act.value = "activate";
	document.d.hash.value = hash;
	document.d.submit();
}
function deactivateQuery(hash)
{
	document.d.act.value = "deactivate";
	document.d.hash.value = hash;
	document.d.submit();
}

// ---------------------------------------------------------------------------
//	メッセンジャー画面用
// ---------------------------------------------------------------------------
function deleteIM()
{
	document.d.submit();
}

// ---------------------------------------------------------------------------
//	dat画面用
// ---------------------------------------------------------------------------
function getBoards()
{
	location.href = "/xml/getboards";
}
function checkCategory(obj, name)
{
	elm = document.getElementById(name);
	elms = elm.getElementsByTagName("input");

	var flag;
	for (i = 0; i < elms.length; i++) {
		if (elms[i].type == "checkbox") {
			if (i == 0) flag = !elms[i].checked;
			elms[i].checked = flag;
		}
	}

	obj.checked = false;
	obj.blur();
}
function checkAllBoard(flag)
{
	elms = document.getElementsByTagName("input");
	for (i = 0; i < elms.length; i++) {
		if (elms[i].type == "checkbox" && elms[i].name != "")
			elms[i].checked = flag;
	}
}
function openThreadList(dom, bbs)
{
	with (top.work.document.f) {
		domain.value = dom;
		bbsname.value = bbs;
		window.top.main.makeContents(selmenu.value);
	}
}
function deleteThread()
{
	document.d.action =	"/xml/thread?domain=" + top.work.document.f.domain.value + "&bbsname=" + top.work.document.f.bbsname.value;
	document.d.submit();
}
function openDat(hash)
{
	var url = "/xml/dat?hash="+hash;
	window.open(url, "_blank");
}

// ---------------------------------------------------------------------------
//	notify画面用
// ---------------------------------------------------------------------------
function notifyIM()
{
	var elm = window.parent.menu.document.getElementById("im");
	elm.className = "notify";
}
function notifyNewVer()
{
	var elm = window.parent.menu.document.getElementById("notifymsg");
	elm.innerHTML = "新バージョンを検知しました";
}

// ---------------------------------------------------------------------------
//	その他
// ---------------------------------------------------------------------------
function showLoadingImage()
{
	var ldimg = parent.menu.document.getElementById("loading");
	ldimg.style.display = "block";
}

function hideLoadingImage()
{
	var ldimg = parent.menu.document.getElementById("loading");
	ldimg.style.display = "none";
}
function selectText(elm)
{
	if (document.createRange) {
		var s = window.getSelection();
		if (s.rangeCount > 0)
			s.removeAllRanges();
		var range = document.createRange();
		range.selectNode(elm);
		s.addRange(range);
	}
	else  {
		var r = document.body.createTextRange();
		r.moveToElementText(elm);
		r.select();
	}
}
function checkAllItem(flag)
{
	with (document.d) {
		elms = getElementsByTagName("input");
		for (i = 0; i < elms.length; i++) {
			if (elms[i].type == "checkbox")
				elms[i].checked = flag;
		}
	}
}

// ---------------------------------------------------------------------------
//	parseErrorHandler
//	XMLパーサ用エラーハンドラ
// ---------------------------------------------------------------------------
function parseErrorHandler(msg)
{
	hideLoadingImage();
	alert('XML Parseエラー:\n'+msg);
}
