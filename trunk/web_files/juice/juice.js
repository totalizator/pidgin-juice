var Buddies = {
	
	buddies_collection: [],
	update_buddy_callbacks: [],
	
	add_buddy: function(buddy) {
		if (this.buddies_collection[buddy.buddyname] == undefined)
			this.buddies_collection[buddy.buddyname] = new Array();
		
		this.buddies_collection[buddy.buddyname].push(buddy);
		this.init_buddy(buddy);
	},
	
	remove_buddy: function(buddy) {
		if (this.buddies_collection[buddy.buddyname] == undefined)
			return;
		for(j=0; j<this.buddies_collection[buddy.buddyname].length; j++)
		{
			if(this.buddies_collection[buddy.buddyname][j].proto_id == buddy.proto_id
			  && this.buddies_collection[buddy.buddyname][j].account_username == buddy.account_username)
			{
				this.buddies_collection[buddy.buddyname].splice(j, 1);
				break;
			}
		}
	},
	
	get_buddy: function(buddyname, proto_id, account_username) {
		if (typeof(buddyname) == "object") {
			account_username = buddyname.account_username;
			proto_id = buddyname.proto_id;
			buddyname = buddyname.buddyname;
		}
		if(this.buddies_collection[buddyname] == undefined)
			return false;
			
		//if it already exists in the list
		existing_buddy = false;
		for(j=0; j<this.buddies_collection[buddyname].length; j++)
		{
			if(this.buddies_collection[buddyname][j].proto_id == proto_id
			  && this.buddies_collection[buddyname][j].account_username == account_username)
			{
				existing_buddy=this.buddies_collection[buddyname][j];
				break;
			}
		}
		existing_buddy.username = existing_buddy.buddyname;
		return existing_buddy;
	},
	
	update_buddy: function(buddyname, proto_id, account_username, buddy_details) {
		//assume (buddy, buddy_details) was passed
		if (buddy_details == undefined) {
			buddy_details = proto_id;
			account_username = buddyname.account_username;
			proto_id = buddyname.proto_id;
			buddyname = buddyname.buddyname;
		}
		
		//update the buddy
		buddy = this.get_buddy(buddyname, proto_id, account_username);
		var i;
		for(i in buddy_details) {
			buddy[i] = buddy_details[i];
		}
		
		//update anything else that needs it
		for(i=0; i<this.update_buddy_callbacks.length; i++) {
			this.update_buddy_callbacks[i](buddy);
		}
	},
	
	add_update_buddy_callback: function(func) {
		if (func == undefined)
			return;
		this.update_buddy_callbacks.push(func);
	},
	
	call_update_buddy_callbacks: function(buddy) {
		for(i=0; i<this.update_buddy_callbacks.length; i++) {
			this.update_buddy_callbacks[i](buddy);
		}
	},
	
	init_buddy: function(buddy) {
		buddy.add_message = function(message){return Buddies.buddy_add_message(this, message);};
		buddy.add_history = function(messages){return Buddies.buddy_add_history(this, messages);};
		buddy.get_messages = function(){return Buddies.buddy_get_messages(this);};
		buddy.unread = 0;
		buddy.is_typing = false;
	},
	
	buddy_add_message: function(buddy, message) {
		//message.test = ["test"];
		//alert(Json.encode(message.test));
		//message.test = "test";
		//alert(Json.encode(message)); return;
		if(buddy.messages == undefined)
			buddy.messages = [];
		buddy.messages.push(message);
		//while testing, only "sent" messages were in the ajax. put this if-condition back in when they're working properly again!
		if (message.type == 'received')
			buddy.unread++;
		this.call_update_buddy_callbacks(buddy);
	},
	buddy_add_history: function(buddy, messages) {
		buddy.messages = messages.reverse();
		this.call_update_buddy_callbacks(buddy);
	},
	buddy_get_messages: function(buddy) {
		if (buddy.messages == undefined)
			return [];
		else
			return buddy.messages;
	}
}


/* Helpers */

function get_style(el, prop) {
	if (document.defaultView && document.defaultView.getComputedStyle) {
		return document.defaultView.getComputedStyle(el, null)[prop];
	} else if (el.currentStyle) {
		return el.currentStyle[prop];
	} else {
		return el.style[prop];
	}
}

var Ajax = {
	get: function(url, func) {
		var req = new XMLHttpRequest();
	
		req.onreadystatechange = function() {
			if(req.readyState == 4) {
				//alert(req.status);
				if(req.status == 200 && req.responseText) {
					if(func != undefined)
						func(req.responseText);
				}
				else {
					//alert("status: " + req.status);
				}
			}
			else {
				//alert("readyState: " + req.readyState);
			}
		};
		
		req.open("GET", url, true);
		req.setRequestHeader( "If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT" );
		req.send(null);
		
		return req;
	},
		
	post: function (url, post_vars, func) {
		var req = new XMLHttpRequest();
	
		req.onreadystatechange = function() {
			if(req.readyState == 4) {
				if(req.status == 200 && req.responseText) {
					if(func != undefined)
						func(req.responseText);
				}
			}
		};
		
		req.open("POST", url, true);
		req.setRequestHeader( "If-Modified-Since", "Sat, 1 Jan 2000 00:00:00 GMT" );
		req.send(post_vars);
		
		return req;
	}
}

var Json = {
	decode: function(text) {
		try {
		if (window.JSON && JSON.parse)
			return JSON.parse(text);
		} catch (e) {}
		if (window.parseJSON == undefined) {
			text = text.substring(text.indexOf('{'), text.lastIndexOf('}')+1);
			return !(/[^,:{}\[\]0-9.\-+Eaeflnr-u \n\r\t]/.test(
				text.replace(/"(\\.|[^"\\])*"/g, ''))) &&
				eval('(' + text + ')');
		}
		else {
			return window.parseJSON(text);
		}
	},
	
	encode: function(obj, depth) {
		if (window.JSON && JSON.stringify)
			return JSON.stringify(obj);
		if (depth==undefined)
			depth = 1;
//		if (depth !== 0)
//			alert(depth);
		
		if (typeof(obj) == "string")
			return '"' + obj + '"';
		else if (typeof(obj) == "number")
			return obj;
		else if (obj === null)
			return 'N';
		
		var values = [];
		var keys = [];
		var iterator = 0;
		var is_array = true;
		var i;
		for(key in obj) {
			if (key != iterator++ && key != length) {
				is_array = false;
			}
			keys.push(key);
			values.push(obj[key]);
		}
		var response = '';
		if (is_array) {
			for(i=0; i<values.length; i++) {
				if (depth <= 0)
					response += ',' + this.encode(null, 0);
				else
					response += ',' + this.encode(values[i], depth-1);
			}
			response = response.replace(/^,/, '');
			response = '[' + response + ']';
		}
		else {
			for(i=0; i<values.length; i++) {
				if (depth <= 0)
					response += ',"' + keys[i] + '":' + this.encode(null, 0);
				else
					response += ',"' + keys[i] + '":' + this.encode(values[i], depth-1);
			}
			response = response.replace(/^,/, '');
			response = '{' + response + '}';
		}
		return response;
	}
}





/* DISPLAY FUNCTIONS */


Buddies.add_update_buddy_callback(update_unread_count);
function update_unread_count(buddy) {
	chat = document.getElementById('chat');
	if ((buddy == Buddies.get_buddy(chat.buddy) && current_page.id=='chat') || buddy.unread < 1) {
		//remove the number from the contact list
		spans = buddy.li.getElementsByTagName('span');
		if (spans.length > 0)
			spans[0].parentNode.removeChild(spans[0]);
		buddy.unread = 0;
	}
	else {
		//add the number to the front screen
		spans = buddy.li.getElementsByTagName('span');
		if (spans.length < 1) {
			span = document.createElement('span');
			span.className = "unread";
			buddy.li.insertBefore(span, buddy.li.firstChild);
			spans = [span];
		}
		spans[0].innerHTML = '(' + buddy.unread + ')';
	}
}

function send_bar_typing_callback(event) {
	if (event.keyCode != 10 && event.keyCode != 13)
		return true;
	
	
	send_message();
	
	//event.target.blur();
	
	return false;
}


function buddy_set_typing(buddy, is_typing) {
	if(is_typing == undefined)
		is_typing = false;
		
	buddy.is_typing = is_typing;
	
	chat = document.getElementById('chat');
	lis = chat.getElementsByTagName('ul')[0];
	if(chat.buddy != undefined) {
		if(buddy != chat.buddy)
			return;
	}
	//remove existing typing notifications if any
	typing_notifications = [];
	for(i=lis.childNodes.length-1; i>=0; i--) {
		if (lis.childNodes[i].className != "typing")
			continue;
		else
			typing_notifications.push(lis.childNodes[i]);
	}
	for(i=0; i<typing_notifications.length; i++) {
		typing_notifications[i].parentNode.removeChild(typing_notifications[i]);
	}
	//add one back on
	if (is_typing) {
		li = document.createElement('LI');
		li.innerHTML = 'typing';
		li.className = 'typing';
		lis.appendChild(li);
	}
}

function update_chat(buddy) {
	chat = document.getElementById('chat');
	if (chat.buddy != buddy)
		return;
	/*chat.buddy = buddy;*/
	
	chat.getElementsByTagName('h1')[0].innerHTML = buddy.display_name;
	
	chat_ul = chat.getElementsByTagName('ul')[0];
	
	//TODO: don't just remove all of the messages, find a way to just add the
	// new ones
	while(chat_ul.children.length)
		chat_ul.removeChild(chat_ul.firstChild);
	
	escaping_div = document.createElement('div');
	messages = buddy.get_messages();
	var i;
	for(i=0; i<messages.length; i++) {
		li = document.createElement('li');
		li.className = messages[i].type
		span = document.createElement('span');
		messages[i].message = messages[i].message.replace(/<BR>/ig, "\n");
		escaping_div.innerHTML = messages[i].message;
		if (escaping_div.textContent != undefined) {
			span.textContent = escaping_div.textContent;
		}
		else {
			span.innerText = escaping_div.innerText;
		}
		li.appendChild(span);
		chat_ul.appendChild(li);
	}
	//delete escaping_div;
	
	//also update the contact page
	contact = document.getElementById('contact');
	contact_div = contact.getElementsByTagName('div')[1];
	contact.getElementsByTagName('h1')[0].innerHTML = buddy.display_name;
	contact_div.innerHTML = buddy.display_name;
	if (buddy.status_message != undefined)
		contact_div.innerHTML += "<br/>"+ buddy.status_message;
	contact_div.innerHTML += "<br/>";
	contact_div.innerHTML += "<br/>"+ buddy.proto_name;
	contact_div.innerHTML += "<br/>"+ buddy.buddyname;;
	contact.getElementsByTagName('img')[0].src = '/buddy_icon.png?proto_id=' + buddy.proto_id + '&proto_username=' + buddy.account_username + '&buddyname=' + buddy.buddyname;

}

Buddies.add_update_buddy_callback(update_chat);
function show_chat(buddy) {
	chat = document.getElementById('chat');
	chat.buddy = buddy;
	update_chat(buddy);
	get_history(buddy);
	change_page('chat', 1);
	document.getElementById('send_bar').getElementsByTagName('textarea')[0].focus();
}


function scroll_to_bottom(buddy) {
	chat = document.getElementById('chat');
	if (chat.buddy != buddy)
		return;
	if (current_page != chat)
		return;
	
	document.getElementById('send_bar').scrollIntoView(false);
}
Buddies.add_update_buddy_callback(scroll_to_bottom);


/* DATA FUNCTIONS */
function send_message() {
	var page = '/send_im.js';
	var chat = document.getElementById('chat');
	var buddy = chat.buddy;
	var textarea = chat.getElementsByTagName('textarea')[0];
	var message = textarea.value;
	textarea.value = "";
	
	page += '?';
	page += 'buddyname='+encodeURIComponent(buddy.buddyname)+'&username='+encodeURIComponent(buddy.buddyname); //backward compatibility
	page += '&proto_id='+encodeURIComponent(buddy.proto_id);
	page += '&account_username='+encodeURIComponent(buddy.account_username);
	page += '&message='+encodeURIComponent(message);
	
	//This is currently using both POST and GET. Probably only the GET values are
	//being read server-side.
	//When server is updated, remove the get component
	
	//Should these all be url encoded? if so, who will decode them? if not, extra & and = will split it apart!;
	Ajax.post(page, "");
}

var events_timeout = 0;
//var events_timeout_long = 0;
function get_events_timeout(delay) {
	if (events_timeout !== 0) {
		clearTimeout(events_timeout);
		events_timeout = 0;
		//clearTimeout(events_timeout_long);
	}
	//events_timeout_long = setTimeout(function() {get_events(); events_timeout_long = 0;}, 60000);
	events_timeout = setTimeout(function() {get_events(); }, delay);
}

function get_history_callback(response) {
	json = Json.decode(response);
	
	buddy = Buddies.get_buddy(json.buddyname, json.proto_id, json.account_username);
	buddy.add_history(json.history);
	
	if (json.history.length > 0 && json.history[0].timestamp != undefined) {
		if (json.history[0].timestamp > latest_event_timestamp) {
			latest_event_timestamp = json.history[0].timestamp;
		}
	}
}

var unprocessed_events = [];
function get_events_callback(response) {
	//alert(response);
	if (events_timeout !== 0) {
		clearTimeout(events_timeout);
	}
	events_timeout = 0;
	
	json = Json.decode(response);
	
	if (json.events.length > 0) {
		event = json.events[json.events.length-1];
		if (event.timestamp != undefined) {
			if (event.timestamp > latest_event_timestamp) {
				latest_event_timestamp = event.timestamp;
			}
		}
	}
	
	json.events.join(unprocessed_events);
	unprocessed_events = [];
	
	var j;
	var needNewBuddyList = false;
	for(j=0; j<json.events.length; j++) {
		event = json.events[j];
		switch(event.type) {
			case "sent" : 
			case "received" : {
				buddy = Buddies.get_buddy(event.buddyname, event.proto_id, event.account_username);
				if(!buddy) {
					//alert("no such buddy "+event.buddyname+" to receive a message");
					needNewBuddyList = true;
					unprocessed_events.push(event);
					break;
				}
				buddy.add_message(event);
				break;
			}
			case "typing" : {
				buddy = Buddies.get_buddy(event.buddyname, event.proto_id, event.account_username);
				if (!buddy) {
					needNewBuddyList = true;
					unprocessed_events.push(event);
					break;
				}
				buddy_set_typing(buddy, true);
				break;
			}
			case "not_typing" : {
				buddy = Buddies.get_buddy(event.buddyname, event.proto_id, event.account_username);
				if (!buddy) {
					needNewBuddyList = true;
					unprocessed_events.push(event);
					break;
				}
				buddy_set_typing(buddy, false);
				break;
			}
		}
	}
	if (needNewBuddyList)
	{
		get_buddies();
	}
}

Buddies.add_update_buddy_callback(update_buddy_in_buddy_list);
function update_buddy_in_buddy_list(buddy) {
		var li = buddy.li;
		var a = li.getElementsByTagName('a')[0];
		a.innerHTML = buddy.display_name;
		if (buddy.available) {
			a.className = "";
		}
		else {
			a.className = "away";
		}
}

function add_buddy_to_buddies_list(buddy) {
	contacts_ul = document.getElementById('contacts').getElementsByTagName('UL')[0];
	li = document.createElement('LI');
	a = document.createElement('A');
	a.innerHTML = buddy.display_name;
	a.href='#';
	if (!buddy.available)
		a.className = "away";
	a.onclick = function(e){show_chat(buddy)};
	img = document.createElement('IMG');
	img.onerror = function(){ this.style.display='none'; };
	img.src = '/protocol/'+buddy.prpl_icon+'.png';
	li.appendChild(img);
	li.appendChild(a);
	contacts_ul.appendChild(li);
	
	buddy.li = li;
	li.buddy = buddy;
}

function get_buddies_callback(response) {
	json = Json.decode(response);
	
	//var badBuddies = new Buddies;
	for(i=0; i< json.buddies.length; i++) {
		buddy = Buddies.get_buddy(json.buddies[i]);
		if (!buddy) {
			Buddies.add_buddy(json.buddies[i]);
			add_buddy_to_buddies_list(json.buddies[i]);
		}
		else {
			//alert(Json.encode(json.buddies[i]));
			Buddies.update_buddy(buddy, json.buddies[i]);
		}
		//badBuddies.remove_buddy(buddy);
	}
	/*var chat = document.getElementById('chat');
	for(i in badBuddies.buddies_collection)
	{
		for(j=0; j<badBuddies.buddies_collection[i].length; j++)
		{
			var buddy = badBuddies.buddies_collection[i][j];
			buddy.li.parentNode.removeChild(buddy.li);
			if (chat.buddy == Buddies.get_buddy(buddy))
			{
				if (current_page == chat)
					change_page("contacts", -1);
				chat.buddy = null;
			}
			Buddies.remove_buddy(buddy);
		}
	}*/
}

function get_history(buddy) {
	url = '/history.js?buddyname='+buddy.buddyname+'&proto_id='+buddy.proto_id+'&proto_username='+buddy.account_username;
	Ajax.get(url, get_history_callback);
}


var latest_event_timestamp = 0;
var eventRequest = 0;
function get_events() {
  if (window.EventSource)
  {
	var eventRequest = new EventSource('/events.js?eventstream=1&timestamp='+latest_event_timestamp);
	eventRequest.onmessage = function(event) {
		get_events_callback(event.data);
	};
	eventRequest.onerror = function() {
		setTimeout(get_events, 1000);
	}
  } else {
	get_events_timeout(60000);
	latest_event_timestamp_magnitude = (latest_event_timestamp+"").length;
	//alert(latest_event_timestamp_magnitude + "\n" + latest_event_timestamp);
	if (latest_event_timestamp > 0 && latest_event_timestamp_magnitude <= 10) {
		//alert("multiplying");
		latest_event_timestamp = latest_event_timestamp * 1000;
	}
	eventRequest = Ajax.get('/events.js?timestamp='+latest_event_timestamp, get_events_callback);
  }
}
function checkEventRequestStatus() {
	if (!eventRequest)
		return;
	if (eventRequest.readyState == 4)
		get_events();
}
setInterval(checkEventRequestStatus, 1000);

function get_buddies() {
	Ajax.get('buddies_list.js', get_buddies_callback);
}


var current_page = false;
function current_page_init() {
	for(i=0; i<document.body.childNodes.length; i++) {
		if (document.body.childNodes[i].tagName == undefined)
			continue;
		if (document.body.childNodes[i].tagName != 'DIV')
			continue;
		if (get_style(document.body.childNodes[i], 'display') == 'block') {
			current_page = document.body.childNodes[i];
		}
	}
}
function change_page(to, direction) {
	if (direction == undefined)
		direction = 0;
		
	//scrolling isn't working smoothly, so force it to snap
	direction = 0;
		
	to_page = false;
	if (to.id != undefined)
		to_page = to;
	else
		to_page = document.getElementById(to);
	/*
	current_page = false;
	for(i=0; i<document.body.childNodes.length; i++) {
		if (document.body.childNodes[i].tagName == undefined)
			continue;
		if (document.body.childNodes[i].tagName != 'DIV')
			continue;
		if (!to_page && document.body.childNodes[i].id == to)
			to_page = document.body.childNodes[i];
		if(get_style(document.body.childNodes[i], 'display') == 'block') {
			current_page = document.body.childNodes[i];
		}
	}
	*/
	
	//if there's nothing to change to, don't change
	if (!to_page)
		return;
	
	//why? we're just saying if no page is displayed, force the next one instantly
	if (!current_page)
		direction = 0;
		
	if (direction < 0) {
		//moving to the page to the left, so swipe left to right
		current_page.style.zIndex = '0';
		to_page.style.zIndex = '1';
		to_page.style.left = '-320px';
		to_page.style.display = 'block';
		//start_tween_px(current_page, 'left', 320);
		//start_tween_px(to_page, 'left', 0);
	}
	else if (direction > 0) {
		current_page.style.zIndex = '0';
		to_page.style.zIndex = '1';
		to_page.style.left = '320px';
		to_page.style.display = 'block';
		//start_tween_px(current_page, 'left', -320);
		//start_tween_px(to_page, 'left', 0);
	}
	else {
		to_page.style.left = '0px';
		to_page.style.top = '0px';
		if (current_page)
			current_page.style.display = 'none';
		to_page.style.display = 'block';
	}
	current_page = to_page;
}

function start_tween_px(el, property, set_point) {
	if (isNaN(set_point)) {
		if (!set_point.match(/px/))
			return;
		set_point = set_point.replace(/px/, '');
	}
	set_point = Math.round(1 * set_point);
	
	var present_value = get_style(el, 'left');
	present_value = Math.round(1 * present_value.replace(/px/, ''));
	
	var direction = 1;
	if (set_point < present_value)
		direction = -1;
		
	setTimeout(tween_px_internal, 0, el, property, direction, present_value, set_point, 10, 1);
}
function tween_px_internal(el, property, direction, pv, sp, step, time) {
	if (direction > 0 && pv >= sp) {
		return;
	}
	else if (direction < 0 && pv <= sp) {
		return;
	}
	pv = pv + (step * direction);
	el.style[property] = pv + 'px';
	setTimeout(tween_px_internal, time, el, property, direction, pv, sp, step, time);
}




addEventListener("load", function(event){
	setTimeout(get_buddies, 0);
	setTimeout(get_events, 0);
	setTimeout(current_page_init, 0);
}, false);
//debugging help
window.chat = document.getElementById('chat');
