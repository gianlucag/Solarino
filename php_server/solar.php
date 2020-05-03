<?php
/********************************************************
* 
* SolarFI ver 1.3.0 - Server PHP
* 
* - Colleziona e salva i dati provenienti dalle schede NodeMCU remote
* - Mostra una pagina HTML di produzione giornaliera
*
* 21/05/2018
* Gianluca Ghettini 
*
********************************************************/




/********************************************************
* 
* Mappa un seriale inverter ad un nome testuale
*
********************************************************/
function getMapping()
{
	$mapping = array(
		"1003BW0063" => "Inverter 1",
		"1005BW0031" => "Inverter 2",
		"0908BW0006" => "Inverter 3",
		"0908BW0012" => "Inverter 4",
		"0908BW0031" => "Inverter 5",
		"0910BW0078" => "Inverter 6",
		"1005BW0029" => "Inverter 7",
		"0908BW0022" => "Inverter 8",
		"1005BW0028" => "Inverter 9",
		"1003BW0061" => "Inverter 10",	
		"1005BW0026" => "Inverter 11",
		"0910BW0035" => "Inverter 12",
		"0908BW0026" => "Inverter 13"
	);

	return $mapping;
}





/********************************************************
* 
* Converte un nibble esadecimale in formato stringa
* nel suo valore numero binario
*
* Es.
* "A" => 10
*
********************************************************/
function hex2val($val)
{
	$val = strtoupper($val);
	if($val == '0') return 0;
	if($val == '1') return 1;
	if($val == '2') return 2;
	if($val == '3') return 3;
	if($val == '4') return 4;
	if($val == '5') return 5;
	if($val == '6') return 6;
	if($val == '7') return 7;
	if($val == '8') return 8;
	if($val == '9') return 9;
	if($val == 'A') return 10;
	if($val == 'B') return 11;
	if($val == 'C') return 12;
	if($val == 'D') return 13;
	if($val == 'E') return 14;
	if($val == 'F') return 15;	
}



/********************************************************
* 
* Converte una stringa esadecimale in un array di byte
*
* Es.
* "02ACFF" => 0x20, 0xAC, 0xFF
*
********************************************************/
function h2b($data)
{
	$res = array();

	for($d = 0; $d < (strlen($data) / 2); $d++)
	{
		$val = (hex2val($data[$d * 2 + 0]) * 16) + hex2val($data[($d * 2) + 1]);
		array_push($res, $val);
	}

	return $res;
}




/********************************************************
* 
* Decodifica del payload dell'inverter
*
********************************************************/
function decode($payload)
{
	$temp = (($payload[0] * 256) + $payload[1]) / 10;

	$vpv1 = (($payload[2] * 256) + $payload[3]) / 10;
	$vpv2 = (($payload[4] * 256) + $payload[5]) / 10;
	$vpv3 = (($payload[6] * 256) + $payload[7]) / 10;

	$etotal = (($payload[14] * 256 * 256 * 256) + ($payload[15] * 256 * 256) + ($payload[16] * 256) + $payload[17]) / 10;
	$htotal = (($payload[18] * 256 * 256 * 256) + ($payload[19] * 256 * 256) + ($payload[20] * 256) + $payload[21]);

	$pac = ($payload[22] * 256) | $payload[23];

	$mode = ($payload[38] * 256) | $payload[39];

	$etoday = (($payload[26] * 256) | $payload[27]) / 100;

	$iac1 = (($payload[42] * 256) | $payload[43]) / 10;
	$vac1 = (($payload[44] << 8) | $payload[45]) / 10;
	$fac1 = (($payload[46] << 8) | $payload[47]) / 100;

	$iac2 = (($payload[58] << 8) | $payload[59]) / 10;
	$vac2 = (($payload[60] << 8) | $payload[61]) / 10;
	$fac2 = (($payload[62] << 8) | $payload[63]) / 100;

	$iac3 = (($payload[74] << 8) | $payload[75]) / 10;
	$vac3 = (($payload[76] << 8) | $payload[77]) / 10;
	$fac3 = (($payload[78] << 8) | $payload[79]) / 100;

	$res = array(
		"temp" => $temp,
		"vpv1" => $vpv1,
		"vpv2" => $vpv2,
		"vpv3" => $vpv3,
		"etotal" => $etotal,
		"htotal" => $htotal,		
		"pac" => $pac,
		"mode" => $mode,
		"etoday" => $etoday,
		"iac1" => $iac1,
		"vac1" => $vac1,
		"fac1" => $fac1,
		"iac2" => $iac2,
		"vac2" => $vac2,
		"fac2" => $fac2,		
		"iac3" => $iac3,
		"vac3" => $vac3,
		"fac3" => $fac3		
	);

	return $res;
}




if(isset($_GET["hello"])) // ping keepalive dalla board NodeMCU
{
	file_put_contents("last", time());
}
else if(isset($_GET["debug"])) // debug dalla board NodeMCU, scrivi sul server la nuova riga di debug
{
	file_put_contents("debug.txt", "\n".json_encode($_GET), FILE_APPEND);
}
else if(isset($_GET["data"])) // arriva un payload dalla board NodeMCU remota. Salva il payload sul server
{
	$now = time();
	$chunks = explode(":", $_GET["data"]);
	$id = $chunks[0];
	$payload = $chunks[1];

	if($payload != "")
	{		
		if(file_exists($id.".txt"))
		{
			// recupera stato inverter corrente
			$dd = decode(h2b($payload));
			$currmode = $dd["mode"];

			// recupera stato inverter precedente
			$raw = json_decode(file_get_contents($id.".txt"));
			$dd = decode(h2b($raw->payload));
			$lastmode = $dd["mode"];

			// recupera nome inverter
			$map = getMapping();
			$idinverter = $id;
			$nomeinverter = $map[$id];

			if($currmode == 0 && $lastmode != 0) // l'allarme è ritornato
			{
				// fai qualcosa
			}

			if($currmode != 0 && $lastmode == 0) // allarme!
			{
				// fai qualcosa
			}
		}

		// salva il payload sul server
		$data = array(
			"name" => $id,
			"time" => $now,
			"payload" => $payload
		);
		file_put_contents($id.".txt", json_encode($data));
		file_put_contents("last", time());
	}
}
else // l'utente accede al pagina di produzione, mostra HTML e dati di produzione
{
	echo "<html><body>";
	
	if(file_exists("last"))
	{
		$last = file_get_contents("last");
		echo "Ultima connessione: ".date("d-m-Y H:i", $last);
	}

	$m = getMapping();
	foreach($m as $k => $v)
	{
		if(file_exists($k.".txt"))
		{
			$raw = json_decode(file_get_contents($k.".txt"));
			
			$now = time();
			$name = $raw->name;
			$time = $raw->time;
			$data = decode(h2b($raw->payload));

			if(($now - $time) > 3600) // sono 30 minuti che l'inverter non risponde
			{
				$fcolor = "#000000";
				$bcolor = "#dddddd";
				$state = "NON RISPONDE";
			}
			else if($data["mode"] == 0) // l'inverter è attivo
			{
				$fcolor = "#000000";
				$bcolor = "#ddffdd";
				$state = "OK";
			}
			else if($data["mode"] > 0) // l'inverter è attivo ma in allarme
			{
				$fcolor = "#ffffff";
				$bcolor = "#E53935";
				$state = "ERRORE";
			}

			echo "<div style='font-size:150%;width:100%;margin:5px;color:".$fcolor.";background-color:".$bcolor.";'>";
			echo "<b>".$v."</b> (".$k.")";
			echo "<br />";
			echo "Ultimo aggiornamento: ".date("d-m-Y H:i", $time);
			echo "<br />";
			echo "<table style='font-size:150%;color:".$fcolor."' width='100%' border='1' cellspacing='0'>";
			echo "<tr>";
			echo "<td>Iac1:".$data["iac1"]."</td><td>Vac1:".$data["vac1"]."</td><td>Fac1:".$data["fac1"]."</td>";
			echo "</tr>";
			echo "<tr>";			
			echo "<td>Iac2:".$data["iac2"]."</td><td>Vac2:".$data["vac2"]."</td><td>Fac1:".$data["fac2"]."</td>";
			echo "</tr>";
			echo "<tr>";
			echo "<td>Iac3:".$data["iac3"]."</td><td>Vac3:".$data["vac3"]."</td><td>Fac1:".$data["fac3"]."</td>";
			echo "</tr>";
			echo "</table>";

			echo "<br />Pac:".$data["pac"]." W";
			echo "<br />Temp:".$data["temp"]." C";
			echo "<br />E-today:".$data["etoday"]." kWh";
			echo "<br />stato: ".$state;
			echo "</div>";
			echo "<br />";
		}
	}

	echo "</body></html>";
}

?>
