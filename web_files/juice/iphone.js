function get_events_callback(responseText) {
	alert(responseText);
	var json = eval("(" + responseText + ")");
	var events = json.events;
	var chat = document.getElementById('chat');
	var lis = chat.getElementsByTagName('ul')[0];
	var current_chat_buddy = get_buddy_from_collection(chat.buddy);
	for(i=0; i<events.length; i++) {
		if (events[i] == undefined || events[i].type == undefined)
			continue;
		switch (events[i].type) {
			case "sent" : 
			case "received" : {
				buddy = get_buddy_from_collection(events[i].buddyname, events[i].proto_id, events[i].account_username);
				if(!buddy) {
					alert("no such buddy "+events[i].buddyname);
					continue;
				}
				buddy.history.unshift(events[i]);
				if (buddy == current_chat_buddy) {
					li = document.createElement('li');
					li.className = events[i].type;
					li.innerHTML = events[i].message;
					lis.appendChild(li);
					//lis.scrollTop = lis.scrollHeight;
					scrollTo(0,10000);
				}
				break;
			}
		}
	}
	setTimeout(get_events, 10);
}
function get_events() {
	ajax_get('/events.js', get_events_callback);
}
function newline_or_send(event) {
	if (event.keyCode != 10 && event.keyCode != 13)
		return true;
	
	if (!event.shiftKey) {
		send_message();
		return false;
	}
	
	event.target.value = event.target.value + "\n";
	
	return true;
}
function send_message_callback(responseText) {
	//get_buddy_history();
}
function send_message() {
	var page = '/send_im.js';
	var chat = document.getElementById('chat');
	var buddy = chat.buddy;
	var textarea = chat.getElementsByTagName('textarea')[0];
	var message = textarea.value;
	textarea.value = "";
	
	page += '?';
	page += 'username='+buddy.buddyname;
	page += '&proto_id='+buddy.proto_id;
	page += '&account_username='+buddy.account_username;
	page += '&message='+message;
	//alert(page);
	
	//This is currently using both POST and GET. Probably only the GET values are
	//being read server-side.
	//When server is updated, remove the get component
	
	//Should these all be url encoded? if so, who will decode them? if not, extra & and = will split it apart!;
	ajax_post(page, "", send_message_callback);
}
function alert_buddy(buddy) {
	s = "";
	s += "\nbuddyname: " + buddy.buddyname;
	s += "\nproto_id: " + buddy.proto_id;
	s += "\naccount_username: " + buddy.account_username;
	s += "\n\nAlternative properties sometimes used:";
	s += "\nusername: " + buddy.username;
	s += "\nproto_username: " + buddy.proto_username;
	alert(s);
}
function getStyle(el, prop) {
  if (document.defaultView && document.defaultView.getComputedStyle) {
    return document.defaultView.getComputedStyle(el, null)[prop];
  } else if (el.currentStyle) {
    return el.currentStyle[prop];
  } else {
    return el.style[prop];
  }
}
var chats = [];
function show_chat(buddy) {
	var chat = document.getElementById('chat');
	var chats = document.getElementById('chats');
	
	//chats.getElementsByTagName('ul')[0].appendChild(buddy.li);

	chat.buddy = buddy;
	chat.getElementsByTagName('h1')[0].innerHTML = buddy.display_name;
	
	if (buddy.is_chat_updated == undefined || !buddy.is_chat_updated)
		get_buddy_history(buddy);
	//just update it with blanks
	update_chat_with_history();
	
	
	//show chat history
	change_page('chat');
	chat.getElementsByTagName('textarea')[0].focus();
}
function get_buddy_history(buddy) {
	if (buddy===undefined)
		buddy = document.getElementById('chat').buddy;
	url = '/history.js?buddyname='+buddy.buddyname+'&proto_id='+buddy.proto_id+'&proto_username='+buddy.account_username;
	ajax_get(url, update_buddy_history);
}
function update_buddy_history(response) {
	alert(response);
	var json = eval("(" + response + ")");
	buddy = get_buddy_from_collection(json.buddyname, json.proto_id, json.account_username);
	buddy.history = json.history;
	buddy.history_string = response;
	update_chat_with_history();
}
function update_chat_with_history() {
	var chat = document.getElementById('chat');
	var ul = chat.getElementsByTagName('ul')[0];
	while(ul.childNodes.length > 0)
		ul.removeChild(ul.firstChild);
	var earliest_message = ul.childNodes.length ? ul.childNodes[0] : false;
	if (chat.buddy.history == undefined)
		chat.buddy.history = [];
	for(i=chat.buddy.history.length-1; i>=0; i--) {
		li = document.createElement('LI');
		if (chat.buddy.history[i].type == "sent")
			li.className = "sent";
		li.innerHTML = chat.buddy.history[i].message;
		if (earliest_message)
			ul.insertBefore(li, earliest_message);
		else
			ul.appendChild(li);
	}
	//ul.scrollTop = ul.scrollHeight;
	scrollTo(0,10000);
}
function show_contact(buddy) {
	var contact = document.getElementById('contact');
	contact.buddy = buddy;
	contact.getElementsByTagName('h1')[0].innerHTML = buddy.display_name;
	//contact.getElementsByTagName('img')[0].src = '/buddy_icon.png?proto_id=' + buddy.proto_id + '&proto_username=' + buddy.account_username + '&buddyname=' + buddy.buddyname;
	change_page('contact');
}
//1: swipe forwad; -1: swipe backward; 0: just change
function change_page(to, direction) {
	if (direction==undefined)
		direction = 1;
	else if(direction != 1 || direction != -1)
		direction = 0;
	
	var current_node = false;
	for(i in document.body.childNodes)
	{
		if (i == "length")
			break;
		node = document.body.childNodes[i];
		if(node.style != undefined && getStyle(node, 'display') == "block")
			current_node = node;
	}
	if (current_node == false)
		return;
	if (typeof(to) == "string")
		to = document.getElementById(to);
	if (to == undefined)
		return;
		
	current_node.style.display = 'none';
	to.style.display = 'block';
}
function buddy_sort_callback(a, b) {
	if (a.display_name < b.display_name)
		return -1;
	else if (a.display_name == b.display_name)
		return 0;
	else
		return 1;
}
var update_buddies_timeout = false;
function update_buddies(buddies) {
	
	//eval("var json="+buddies+";");
	var json = eval("(" + buddies + ")");
	var buddies = json.buddies;
	
	if(!buddies.length) {
		buddies_update_timeout = setTimeout(get_buddies, 1000);
		return;
	}
	
	buddies.sort(buddy_sort_callback);
	
	time_updated = new Date().getTime();
	
	var lis = document.getElementById('contacts').getElementsByTagName('UL');
	var buddylist = lis[lis.length-1];
	
	for (var i=0; i<buddies.length; i++)
	{
		buddy = buddies[i];
		existing_buddy=get_buddy_from_collection(buddy.buddyname, buddy.proto_id, buddy.account_username);
		if (!existing_buddy)
		{
			existing_buddy = buddy = create_buddy(buddy);
			add_buddy_to_collection(buddy);
			buddylist.appendChild(buddy.li);
		}
		update_buddy(existing_buddy,buddy);
		buddy.time_updated = time_updated;
		
	}
	
	/* remove contacts that aren't in the list
	for(i=0; i<buddylist.childNodes.length; i++) {
		if(buddylist.childNodes[i].style == undefined)
			continue;
		if (buddylist.childNodes[i].buddy.time_updated < time_updated) {
			alert('old contact');
			b=buddylist.childNodes[i].buddy;
			g=buddylist[b.buddyname];
			for(j=0; j <g.length; j++){
				if(g[j] == b) {
					if (j == g.length-1)
						buddylist[b.buddyname] = g.slice(0, j);
					else
						buddylist[b.buddyname] = g.slice(0, j).concat(g.slice(j+1));
					break;
				}
			}
		}
	}
	*/
	
	buddies_update_timeout = setTimeout(get_buddies, 5000);
}
function create_buddy(buddy) {
	a = document.createElement('A');
	a.href="#";
	a.innerHTML = buddy.display_name;
	a.onclick = function() {show_contact(buddy); return false;};
	a.onclick = function() {show_chat(buddy); return false;};
	li = document.createElement('LI');
	li.appendChild(a);
	li.a=a;
	li.buddy = buddy;
	buddy.li = li;
	return buddy;
}
function update_buddy(buddy_old, buddy_new) {
	li = buddy_old.li;
	li.a.innerHTML = buddy_new.display_name;
	if(buddy_new.available)
		li.a.className = '';
	else {
		li.a.className = 'away';
		li.a.innerHTML += " (away)";
	}
}
var buddies_collection = {};
function add_buddy_to_collection(buddy) {
	buddies_collection[buddy.buddyname].push(buddy);
}
function remove_buddy_from_collection(buddy) {
	if (buddies_collection[buddy.buddyname] == undefined)
		return false;
	for(j=0; j<buddies_collection[buddy.buddyname]; j++) {
		if(buddies_collection[buddy.buddyname][j].proto_id == buddy.proto_id
			&& buddies_collection[buddy.buddyname][j].account_username == buddy.account_username) {
			buddies_collection[buddy.buddyname].splice(j, 1);
			return true;
		}
	}
	return false;
}
function get_buddy_from_collection(buddyname, proto_id, account_username) {
	//if it's an object, find a similar one in the collection
	if(buddyname.buddyname != undefined) {
		account_username = buddyname.account_username;
		proto_id = buddyname.proto_id;
		buddyname = buddyname.buddyname;
	}
	if(buddies_collection[buddyname] == undefined)
	{
		buddies_collection[buddyname] = [];
	}
	//if it already exists in the list
	existing_buddy = false;
	for(j=0; j<buddies_collection[buddyname].length; j++)
	{
		if(buddies_collection[buddyname][j].proto_id == proto_id
		  && buddies_collection[buddyname][j].account_username == account_username)
		{
			existing_buddy=buddies_collection[buddyname][j];
			break;
		}
	}
	existing_buddy.username = existing_buddy.buddyname;
	return existing_buddy;
}
function get_buddies() {
	clearTimeout(update_buddies_timeout);
	ajax_get("buddies_list.js", update_buddies);
}
function ajax_post(page, post_string, func) {		
	var req = null;
	if (window.XMLHttpRequest)
	{
		req = new XMLHttpRequest();
	}
	else if (window.ActiveXObject)
	{
		try {
			req = new ActiveXObject("Msxml2.XMLHTTP");
		} catch (e)
		{
			try {
				req = new ActiveXObject("Microsoft.XMLHTTP");
			} catch (e) {}
		}
	 }

	req.onreadystatechange = function()
	{
		if(req.readyState == 4)
		{
			if(req.status == 200 && req.responseText) {
				if(func != undefined)
					func(req.responseText);
			}
		}
	};
	if (page == undefined)
		page = "";
	if(post_string == undefined)
		post_string = "";
	req.open("POST", page, true);
	req.send(post_string);
}
function ajax_get(page, func)
{
	var req = null;
	if (window.XMLHttpRequest)
	{
		req = new XMLHttpRequest();
	}
	else if (window.ActiveXObject)
	{
		try {
			req = new ActiveXObject("Msxml2.XMLHTTP");
		} catch (e)
		{
			try {
				req = new ActiveXObject("Microsoft.XMLHTTP");
			} catch (e) {}
		}
	 }

	req.onreadystatechange = function()
	{
		if(req.readyState == 4)
		{
			if(req.status == 200 && req.responseText) {
				if(func != undefined)
					func(req.responseText);
			}
		}
	};
	if (!page)
		page = "";
	req.open("GET", page, true);
	req.send(null);

}

addEventListener("load", function(event)
{
	//Uncomment this to enable landscape mode
	setTimeout(checkOrientAndLocation, 300);
	setTimeout(scrollTo, 0, 0, 1);
	setTimeout(get_buddies, 300);
	setTimeout(get_events, 1000);
}, false);
var currentWidth = 0;
function checkOrientAndLocation()
{
	if (window.innerWidth != currentWidth)
	{
		currentWidth = window.innerWidth;

		var orient = currentWidth == 320 ? "profile" : "landscape";
		document.body.setAttribute("orient", orient);
		document.body.className = orient;
		scrollTo(0,1);
	}
	setTimeout(checkOrientAndLocation, 300);
}

function setup_page() {

var animateX = -20;
var animateInterval = 24;

var currentPage = null;
var currentDialog = null;
var currentHash = location.hash;
var hashPrefix = "#_";
var pageHistory = [];


    
addEventListener("click", function(event)
{
    event.preventDefault();

    var link = event.target;
    while (link && link.localName.toLowerCase() != "a")
        link = link.parentNode;

    if (link && link.hash)
    {
        var page = document.getElementById(link.hash.substr(1));
        showPage(page);
    }
}, true);


function checkOrientAndLocation()
{
    if (window.outerWidth != currentWidth)
    {
        currentWidth = window.outerWidth;

        var orient = currentWidth == 320 ? "profile" : "landscape";
        document.body.setAttribute("orient", orient);
    }
		
    if (location.hash != currentHash)
    {
        currentHash = location.hash;

        var pageId = currentHash.substr(hashPrefix.length);
        var page = document.getElementById(pageId);
        if (page)
        {
            var index = pageHistory.indexOf(pageId);
            var backwards = index != -1;
            if (backwards)
                pageHistory.splice(index, pageHistory.length);
                
            showPage(page, backwards);
        }
    }
    
}
    
function showPage(page, backwards)
{
    if (currentDialog)
    {
        currentDialog.removeAttribute("selected");
        currentDialog = null;
    }

    if (page.className.indexOf("dialog") != -1)
        showDialog(page);
    else
    {        
        location.href = currentHash = hashPrefix + page.id;
        pageHistory.push(page.id);

        var fromPage = currentPage;
        currentPage = page;

        var pageTitle = document.getElementById("pageTitle");
        pageTitle.innerHTML = page.title || "";

        var homeButton = document.getElementById("homeButton");
        if (homeButton)
            homeButton.style.display = ("#"+page.id) == homeButton.hash ? "none" : "inline";

        if (fromPage)
            setTimeout(swipePage, 0, fromPage, page, backwards);
    }
}

function swipePage(fromPage, toPage, backwards)
{        
    toPage.style.left = "100%";
    toPage.setAttribute("selected", "true");
    scrollTo(0, 1);
    
    var percent = 100;
    var timer = setInterval(function()
    {
        percent += animateX;
        if (percent <= 0)
        {
            percent = 0;
            fromPage.removeAttribute("selected");
            clearInterval(timer);
        }

        fromPage.style.left = (backwards ? (100-percent) : (percent-100)) + "%"; 
        toPage.style.left = (backwards ? -percent : percent) + "%"; 
    }, animateInterval);
}

function showDialog(form)
{
    currentDialog = form;
    form.setAttribute("selected", "true");
    
    form.onsubmit = function(event)
    {
        event.preventDefault();
        form.removeAttribute("selected");

        var index = form.action.lastIndexOf("#");
        if (index != -1)
            showPage(document.getElementById(form.action.substr(index+1)));
    }

    form.onclick = function(event)
    {
        if (event.target == form)
            form.removeAttribute("selected");
    }
}

}