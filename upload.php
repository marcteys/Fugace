<?php

// https://stackoverflow.com/questions/19447435/ajax-upload-image


$filetype = array('jpeg','jpg','png','gif','PNG','JPEG','JPG');
   foreach ($_FILES as $key )
    {

          $name =time()."-".$key['name'];

          $path='images/'.$name;
          $file_ext =  pathinfo($name, PATHINFO_EXTENSION);
          if(in_array(strtolower($file_ext), $filetype))
          {
            if($key['name']<1000000)
            {

             @move_uploaded_file($key['tmp_name'],$path);
             echo $name;

            }
           else
           {
               echo "FILE_SIZE_ERROR";
           }
        }
        else
        {
            echo "FILE_TYPE_ERROR";
        }// Its simple code.Its not with proper validation.
}

        exit();
?>