require 'icu4r'
str = " abcあいうえおアイウエオアイウエオ漢字,0123スクリプト".u
puts str.inspect_names
p str=~ ure('[\p{Script=Latin}]+')
p str=~ ure('[\p{Script=Hiragana}]+')
p str=~ ure('[\p{Script=Katakana}]+')
p str=~ ure('[\p{Script=Hiragana}\p{Script=Katakana}]+')
p str=~ ure('[\p{blk=CJKUnifiedIdeographs}]+')
p str=~ ure('[\p{L}]+')
p str=~ ure('\u3042') # あ
p str.scan(ure('[\p{N}]'))
