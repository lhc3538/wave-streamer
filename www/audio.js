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



function start(url)
{
    var count = 1000;
    while(count--)
    {
        getSnapshot(url);
    }

}
//------------------------------
var audioCtx = new (window.AudioContext || window.webkitAudioContext)();
var flag = true;
//var source;
function getSnapshot(url)
{
    var source = audioCtx.createBufferSource();
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
        //console.log(source);
        if (flag)
        {
            //flag = false;
            source.start(0);
            console.log("started");
        }

    }

    request.send();
}

function playSnapshot(url)
{
    getSnapshot(url);
    //source.start(0);
}

function getBlob2(data,len){
    var buffer = new ArrayBuffer(len);
    var dataview = new DataView(buffer);
    //writeUint8Array(dataview,0,data,len);
    return new Blob([dataview], { type: 'audio/wav' });
}

var audio = document.querySelector('audio');
var audioContext = new (window.AudioContext || window.webkitAudioContext)();
var str2buf = function (str) {
    var arr = [];
    for (var i = 0; i < str.length; i += 1)
        arr.push(str[i].charCodeAt(0));
    return arr;
};
function playStreamer(url)
{

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = '';
    xhr.send();
    xhr.onreadystatechange = function(e) {
      console.log(xhr);
    };

    xhr.onload = function() {
//        var data =  str2buf(xhr.response);
//        var data =  xhr.response;
        var bb = new BlobBuilder();
        bb.append(xhr.response);
        console.log(data);
        var reader = new FileReader();

        reader.onload = function(evt)
        {
            if(evt.target.readyState == FileReader.DONE)
            {
                console.log(evt.target.result);
                //var data = new Uint8Array(evt.target.result);


                // 方式1 ,ok
                audioContext.decodeAudioData(evt.target.result, function(buffer) {//解码成pcm流
                    var audioBufferSouceNode = audioContext.createBufferSource();
                    audioBufferSouceNode.buffer = buffer;
                    audioBufferSouceNode.connect(audioContext.destination);
                    audioBufferSouceNode.start(0);
                }, function(e) {
                    alert("Fail to decode the file.");
                });


                //方式2 ok
                //audio.src = window.URL.createObjectURL(evt.target.result);
            }
        };
        //var file = new Blob([new Uint8Array(data)], {type: 'audio/wav'});
        reader.readAsArrayBuffer(bb.getBlob());
    };
}
