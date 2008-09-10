require 'icu4r'
res = {}
src = <<-EOT
外国語の勉強と教え
Изучение и обучение иностранных языков
Enseñanza y estudio de idiomas
'læŋɡwidʒ 'lɘr:niŋ ænd 'ti:ʃiŋ
‭‫ללמוד וללמד את השֵפה
L'enseignement et l'étude des langues
Γλωσσική Εκμὰθηση και Διδασκαλία
เรียนและสอนภาษา
EOT
src = src.u
["line_break", "char", "sentence", "word"].each do |brk|
	res[brk] = {}
	["ja",  "en", "th"].each do |loc|
	  out = []
	  src.send("each_#{brk}".to_sym, loc) { |s| out << s }
	  res[brk][loc] = out.join("|")
	  puts "---------#{brk}-------#{loc}---------"
	  puts out.join("|")
	end
end
