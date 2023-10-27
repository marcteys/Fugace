<?php $config['base_url'] = 'http://localhost/phototicket/';
?>
<!DOCTYPE html>
<html>
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<title>Ticket</title>
	<script src="//ajax.googleapis.com/ajax/libs/jquery/2.0.3/jquery.min.js"></script>
	<link rel="stylesheet" type="text/css" href="style.css">
	 <link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=VT323&display=swap" rel="stylesheet"> 
<link
  rel="stylesheet"
  href="https://unpkg.com/dropzone@5/dist/min/dropzone.min.css"
  type="text/css"
/>

<script src="https://unpkg.com/dropzone@5/dist/min/dropzone.min.js"></script>

</head>
<body>


<div class="ticket">
  <div class="ticket__content">
    <h1>Photo Ticket</h1>
<div class="rCol"> 
     <div id ="prv" style="height:auto; width:auto; float:left; margin-bottom: 5px"></div>
       </div>
    <div class="rCol" style="clear:both;">

    <label for="file-upload" class="custom-file-upload" onclick="getFile()">
     Upload Photo
</label>
<input id="file" type="file"  onChange=" return submitForm();"/>
    <input type="hidden" id="filecount" value='0'>
  </div>
</div>




<script>

function getFile(){
     document.getElementById("file").click();
}


	var lastImageDiv = null;


function submitForm() {
    var fcnt = $('#filecount').val();
    var fname = $('#filename').val();
    var imgclean = $('#file');
    if(fcnt<=500)
    {
    data = new FormData();
    data.append('file', $('#file')[0].files[0]);

    var imgname  =  $('input[type=file]').val();
     var size  =  $('#file')[0].files[0].size;

    var ext =  imgname.substr( (imgname.lastIndexOf('.') +1) );
    if(ext=='jpg' || ext=='jpeg' || ext=='png' || ext=='gif' || ext=='PNG' || ext=='JPG' || ext=='JPEG')
    {
     if(size<=10000000)
     {
    $.ajax({
      url: "<?php echo $config['base_url'] ?>/upload.php",
      type: "POST",
      data: data,
      enctype: 'multipart/form-data',
      processData: false,  // tell jQuery not to process the data
      contentType: false   // tell jQuery not to set contentType
    }).done(function(data) {
       if(data!='FILE_SIZE_ERROR' || data!='FILE_TYPE_ERROR' )
       {
        fcnt = parseInt(fcnt)+1;
        $('#filecount').val(fcnt);
        var img = '<div class="dialog" id ="img_'+fcnt+'" ><img src="<?php echo $config['base_url'] ?>/images/'+data+'"><a href="#" id="rmv_'+fcnt+'" onclick="return removeit('+fcnt+')" class="close-classic"></a></div><input type="hidden" id="name_'+fcnt+'" value="'+data+'">';
        $('#prv').append(img);
        lastImageDiv = img;
        if(fname!=='')
        {
          fname = fname+','+data;
        }else
        {
          fname = data;
        }
         $('#filename').val(fname);
          imgclean.replaceWith( imgclean = imgclean.clone( true ) );
       }
       else
       {
         imgclean.replaceWith( imgclean = imgclean.clone( true ) );
         alert('SORRY SIZE AND TYPE ISSUE');
       }

    });
    return false;
  }//end size
  else
  {
      imgclean.replaceWith( imgclean = imgclean.clone( true ) );//Its for reset the value of file type
    alert('Sorry File size exceeding from 1 Mb');
  }
  }//end FILETYPE
  else
  {
     imgclean.replaceWith( imgclean = imgclean.clone( true ) );
    alert('Sorry Only you can uplaod JPEG|JPG|PNG|GIF file type ');
  }
  }//end filecount
  else
  {    imgclean.replaceWith( imgclean = imgclean.clone( true ) );
     alert('You Can not Upload more than 6 Photos');
  }
}
</script>

</body>
</html>