<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<xsl:apply-templates select="result"/>
	</xsl:template>

	<xsl:template match="result">
	  <div id="body" class="body">
      <form method="POST" action="/xml/sql">
  	    SQL:
  	    <input type="button" class="btn" value="POST" onClick="this.disabled = true;document.forms[0].submit();" /><br/>
  	    <textarea name="sql" style="width:100%" rows="5">
  				<xsl:value-of select="sql"/>
  	    </textarea>
      </form>
      <xsl:apply-templates select="count"/>
      <table>
   			<xsl:apply-templates select="row"/>
      </table>
    </div>
	</xsl:template>

	<xsl:template match="row">
	  <tr>
      <xsl:apply-templates select="name"/>
      <xsl:apply-templates select="col"/>
	  </tr>
	</xsl:template>

	<xsl:template match="name">
	  <th>
      <xsl:value-of select="."/>
	  </th>
	</xsl:template>

	<xsl:template match="col">
	  <td>
      <xsl:value-of select="."/>
	  </td>
	</xsl:template>
</xsl:stylesheet>
