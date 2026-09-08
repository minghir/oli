{\rtf1\ansi\deff0 {\fonttbl{\f0 Arial;}}\f0\fs24
\b RAPORT FLOTĂ - {{ $companie }} - {{ $data_curenta | date_format:"d.m.Y H:i" }} \b0\par\par
{{ cycle $fleet as $p }}
\b Inmatriculare:\b0  {{ TRIM($p.registration) }} | \b Denumire:\b0  {{ $p.operator_id }} | \b MTOW:\b0  {{ $p.mtow/1000 | number_format:3:"."}} kg\par
{{ endcycle }}
}
