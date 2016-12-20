DELIMITER $$

DROP FUNCTION IF EXISTS `str_split_unique` $$

CREATE FUNCTION `str_split_unique`(strlist TEXT, chr CHAR(1)) RETURNS TEXT
BEGIN
	DECLARE str TEXT;
	DECLARE pos INT DEFAULT 1;
	DECLARE i INT;
	DECLARE len INT DEFAULT 0;
	DECLARE varReturn TEXT DEFAULT '';
	
	SET len = LENGTH(strlist);
	
	label: LOOP
		SET i = LOCATE(chr, strlist, pos);
		IF i = 0 THEN
			LEAVE label;
		END IF;
		
		SET str = SUBSTRING(strlist, pos, i - pos);
		SET pos = i + 1;
		IF FIND_IN_SET(str, varReturn) = 0 THEN
			SET varReturn = CONCAT(varReturn, chr, str);
		END IF;
	END LOOP;

	RETURN TRIM(BOTH chr FROM CONCAT(varReturn, chr, SUBSTRING(strlist, pos, len-pos+1)));
END $$

DELIMITER ;

SELECT str_split_unique('a,a,b,c,ab,abc,ab,test,abao', ','), str_split_unique('a;a;b;c;ab;abc;ab;test;abao', ';');