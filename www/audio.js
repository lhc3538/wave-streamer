//var audioCtx = new (window.AudioContext || window.webkitAudioContext)();
//var source = audioCtx.createBufferSource();
//var xmlhttp;
//function loadXMLDoc(url)
//{
//    xmlhttp=null;
//    if (window.XMLHttpRequest)
//    {// code for IE7, Firefox, Opera, etc.
//        xmlhttp=new XMLHttpRequest();
//    }
//    else if (window.ActiveXObject)
//    {// code for IE6, IE5
//        xmlhttp=new ActiveXObject("Microsoft.XMLHTTP");
//    }
//    if (xmlhttp!=null)
//    {
//        xmlhttp.onreadystatechange=state_Change;
//        xmlhttp.open("GET",url,true);
//        xmlhttp.responseType = 'arraybuffer';
//        xmlhttp.send(null);
//        source.start(0);
//    }
//    else
//    {
//        alert("Your browser does not support XMLHTTP.");
//    }
//}

//function state_Change()
//{
//    if (xmlhttp.readyState==3)
//    {// 3 = "loading"
//        console.log("1");
//        var audioData = xmlhttp.response;
//        console.log(audioData);
//        audioCtx.decodeAudioData(audioData, function(buffer) {
//            source.buffer = buffer;

//            source.connect(audioCtx.destination);
//            //source.loop = true;
//        });
//        console.log("3");
//    }

//}

//function getData(url) {
//    var request = new XMLHttpRequest();

//    request.open('GET', url, true);

//    request.responseType = 'arraybuffer';

//    /*request.onprogress = function(pe) {
//        //document.getElementById('A1').innerHTML = pe.loaded;
//        //alert(request.response);
//        //document.getElementById('A2').innerHTML = pe.total;
//        document.getElementById('A3').innerHTML = pe.srcElement.response;
//        console.log(pe.srcElement.response);
//    }*/
//    request.onload = function() {
//        var audioData = request.response;
//        //document.getElementById('A3').innerHTML = request.responseText;
//        audioCtx.decodeAudioData(audioData, function(buffer) {
//            source.buffer = buffer;

//            source.connect(audioCtx.destination);
//            //source.loop = true;
//        },

//        function(e){"Error with decoding audio data" + e.err});

//    }

//    request.send();


//}

//// progress on transfers from the server to the client (downloads)
//function updateProgress(evt) {
//    if (evt.lengthComputable) {
//        //var percentComplete = evt.loaded / evt.total;
//        document.getElementById('A1').innerHTML = 1
//    } else {
//        // Unable to compute progress information since the total size is unknown
//        document.getElementById('A2').innerHTML = 2;
//    }
//}



//function start(url)
//{
//    getData(url);
//    source.start(0);

//}
//------------------------------
var audioCtx = new (window.AudioContext || window.webkitAudioContext)();
var source;
function getSnapshot(url)
{
    source = audioCtx.createBufferSource();
    var request = new XMLHttpRequest();

    request.open('GET', url, true);

    request.responseType = 'arraybuffer';


    request.onload = function() {
        var audioData = request.response;
        //console.log(audioData.length);
        audioCtx.decodeAudioData(audioData, function(buffer) {
            source.buffer = buffer;

            source.connect(audioCtx.destination);
            //source.loop = true;
        },

        function(e){ console.log("Error with decoding audio data" + e.err); });
        source.start(0);
    }

    request.send();
}

function playSnapshot(url)
{
    getSnapshot(url);
    //source.start(0);
}

function playStreamer(url)
{
    source = audioCtx.createBufferSource();
    var request = new XMLHttpRequest();

    request.open('GET', url, true);

    request.responseType = 'arraybuffer';

    var audioData = request.response;
    //console.log(audioData.length);
    audioCtx.decodeAudioData(audioData, function(buffer) {
        source.buffer = buffer;

        source.connect(audioCtx.destination);
        //source.loop = true;
    },

    function(e){ console.log("Error with decoding audio data" + e.err); });

    source.start(0);

    request.send();
}
