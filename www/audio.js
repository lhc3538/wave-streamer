var xmlhttp;
function loadXMLDoc(url)
{
    xmlhttp=null;
    if (window.XMLHttpRequest)
    {// code for IE7, Firefox, Opera, etc.
        xmlhttp=new XMLHttpRequest();
    }
    else if (window.ActiveXObject)
    {// code for IE6, IE5
        xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
    }
    if (xmlhttp!=null)
    {
        xmlhttp.onreadystatechange=state_Change;
        xmlhttp.open("GET",url,true);
        xmlhttp.send(null);
    }
    else
    {
        alert("Your browser does not support XMLHTTP.");
    }
}

function state_Change()
{
    if (xmlhttp.readyState==4)
    {// 4 = "loaded"
        if (xmlhttp.status==200)
        {// 200 = "OK"
            document.getElementById('A1').innerHTML=xmlhttp.status;
            document.getElementById('A2').innerHTML=xmlhttp.statusText;
            document.getElementById('A3').innerHTML=xmlhttp.responseText;
        }
        else
        {
            alert("Problem retrieving XML data:" + xmlhttp.statusText);
        }
    }
}

var audioCtx = new (window.AudioContext || window.webkitAudioContext)();
var test = 0;
function getData(url) {
    var source = audioCtx.createBufferSource();
    var request = new XMLHttpRequest();

    request.open('GET', url, true);

    request.responseType = '';

    request.onprogress = function(pe) {
        //document.getElementById('A1').innerHTML = pe.loaded;
        //alert(request.response);
        //document.getElementById('A2').innerHTML = pe.total;
        document.getElementById('A3').innerHTML = pe.srcElement.response;
        console.log(pe.srcElement.response);
    }
    request.onload = function() {
        var audioData = request.response;
        document.getElementById('A3').innerHTML = request.responseText;
        /*audioCtx.decodeAudioData(audioData, function(buffer) {
        source.buffer = buffer;

        source.connect(audioCtx.destination);
        //source.loop = true;
      },

      function(e){"Error with decoding audio data" + e.err});*/

    }

    request.send();
    //source.start(0);


}

// progress on transfers from the server to the client (downloads)
function updateProgress(evt) {
    if (evt.lengthComputable) {
        //var percentComplete = evt.loaded / evt.total;
        document.getElementById('A1').innerHTML = 1
    } else {
        // Unable to compute progress information since the total size is unknown
        document.getElementById('A2').innerHTML = 2;
    }
}



function start(url)
{
    // var count = 5;
    // while(count--){
    getData(url);
    // }
}
