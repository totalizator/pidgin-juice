function getStyle(el, prop) {
  if (document.defaultView && document.defaultView.getComputedStyle) {
    return document.defaultView.getComputedStyle(el, null)[prop];
  } else if (el.currentStyle) {
    return el.currentStyle[prop];
  } else {
    return el.style[prop];
  }
}
function show_chat(buddy) {
	var chat = document.getElementById('chat');
	chat.buddy = buddy;
	chat.getElementsByTagName('h1')[0].innerHTML = buddy.display_name;
	//show chat history
	change_page('chat');
}
function show_contact(buddy) {
	var contact = document.getElementById('contact');
	contact.buddy = buddy;
	contact.getElementsByTagName('h1')[0].innerHTML = buddy.display_name;
	//contact.getElementsByTagName('img')[0].src = '/buddy_icon.png?proto_id=' + buddy.proto_id + '&proto_username=' + buddy.account_username + '&buddyname=' + buddy.username;
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
	clearTimeout(update_buddies_timeout);
	
	eval("var json="+buddies+";");
	var buddies = json.buddies;
	
	buddies.sort(buddy_sort_callback);
	
	time_updated = new Date().getTime();
	
	var buddylist = document.getElementById('contacts').getElementsByTagName('UL')[0];
	for (var i=0; i<buddies.length; i++)
	{
		buddy = buddies[i];
		if(buddylist[buddy.username] == undefined)
		{
			buddylist[buddy.username] = [];
		}
		//if it already exists in the list
		found = false;
		for(j=0; j<buddylist[buddy.username].length; j++)
		{
			if(buddylist[buddy.username][j].proto_id == buddy.proto_id
			  && buddylist[buddy.username][j].account_username == buddy.account_username)
			{
				found=buddylist[buddy.username][j];
				break;
			}
		}
		if (!found)
		{
			found = buddy = create_buddy(buddy);
			buddylist[buddy.username].push(buddy);
			buddylist.appendChild(buddy.li);
		}
		update_buddy(found, buddy);
		buddy.time_updated = time_updated;
		
	}
	/*
		for(i=0; i<buddylist.childNodes.length; i++) {
			if(buddylist.childNodes[i].style == undefined)
				continue;
			if (buddylist.childNodes[i].buddy.time_updated < time_updated) {
				alert('old contact');
				b=buddylist.childNodes[i].buddy;
				g=buddylist[b.username];
				for(j=0; j <g.length; j++){
					if(g[j] == b) {
						if (j == g.length-1)
							buddylist[b.username] = g.slice(0, j);
						else
							buddylist[b.username] = g.slice(0, j).concat(g.slice(j+1));
						break;
					}
				}
			}
		}
		*/
	buddies_update_timeout = setTimeout(get_buddies, 2000);
}
function create_buddy(buddy) {
	a = document.createElement('A');
	a.href="#";
	a.innerHTML = buddy.display_name;
	a.onclick = function() {show_contact(buddy); return false;};
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
function get_buddies() {
	ajax_get("buddies_list.js", update_buddies);
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
			func(req.responseText);
		}
	};
	if (!page)
		page = "";
	req.open("GET", page, true);
	req.send(null);

}

var currentWidth = 0;
addEventListener("load", function(event)
{
    setInterval(checkOrientAndLocation, 300);
    setTimeout(scrollTo, 0, 0, 1);
    setTimeout(get_buddies, 300);
}, false);
function checkOrientAndLocation()
{
    if (window.outerWidth != currentWidth)
    {
        currentWidth = window.outerWidth;

        var orient = currentWidth == 320 ? "profile" : "landscape";
        document.body.setAttribute("orient", orient);
    }
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