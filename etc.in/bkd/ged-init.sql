CREATE DATABASE ged;
CREATE USER 'gedadmin'@'localhost' IDENTIFIED BY 'whaza';
GRANT ALL ON ged.* TO 'gedadmin'@'localhost' IDENTIFIED BY 'whaza';
