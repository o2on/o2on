<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:template match="/">
		<xsl:apply-templates select="messages"/>
	</xsl:template>
	<xsl:template match="messages">
	  <iframe name="bc" src="/xml/imbroadcast" width="100%" height="120px"></iframe>
    <form name="broadcast" method="POST" action="/xml/imbroadcast" target="bc" onSubmit="msg.value=dummy_msg.value;submit();dummy_msg.value='';return false;">
      <input type="text" name="dummy_msg" class="bcmsg" autocomplete="off" style="width:100%"/>
      <input type="hidden" name="msg" value=""/>
    </form>
    <form name="d" method="POST" action="/xml/im">
  	  <div id="body" class="body">
        <div class="infomessage {info/message_type}">
          <xsl:value-of select="info/message"/>
        </div>
      	<table width="100%">
      	  <tr>
      	    <td class="layout" width="33%">
              <input type="button" value="全選択" onClick="checkAllItem(true)"/>
              <input type="button" value="全解除" onClick="checkAllItem(false)"/>
              <input type="button" style="width:120px" value="削除" onClick="deleteIM()"/>
            </td>
            <td class="layout" width="34%">
            </td>
      	    <td class="layout" width="33%">
            </td>
      	  </tr>
      	</table>
      	<table width="100%">
        	<tr>
          	<th></th>
          	<th class="sort" onClick="Sort('date','text')">日時</th>
          	<th class="sort" onClick="Sort('name','text')">名前</th>
          	<th class="sort" onClick="Sort('msg','text')">メッセージ</th>
        	</tr>
    			<xsl:apply-templates select="message">
              <xsl:sort select="SORTKEY" data-type="SORTTYPE" order="SORTORDER"/>
          </xsl:apply-templates>
    		</table>
      </div>
    </form>
	</xsl:template>
	<xsl:template match="message">
	  <xsl:param name="limit" select="LIMIT"/>
    <xsl:if test="position() &lt; $limit">
  		<tr>
  		  <xsl:if test="mine = '1'">
    	    <td class="mine">
    	      <input type="checkbox" class="checkbox" name="{key}" value=""/>
    	    </td>
    			<td class="mine">
    				<xsl:value-of select="date"/>
    			</td>
    			<td class="mine">
            <xsl:if test="port = '0'">
        			<xsl:if test="not(name) or name = ''">
        			  ?
        			</xsl:if>
        			<xsl:if test="name and name != ''">
      	    		<xsl:value-of select="name"/>
        			</xsl:if>
            </xsl:if>
            <xsl:if test="port != '0'">
      			  <a href="javascript:void(0)" onClick="showMsgForm('{id}','{ip}','{port}','{name}','{pubkey}')">
        			  <xsl:if test="not(name) or name = ''">
        			    ?
        			  </xsl:if>
        			  <xsl:if test="name and name != ''">
      	    			<xsl:value-of select="name"/>
        			  </xsl:if>
              </a>
            </xsl:if>
    			</td>
    			<td class="mine">
     				<xsl:value-of select="msg"/>
    			</td>
  		  </xsl:if>
  		  <xsl:if test="mine = '0'">
    	    <td>
    	      <input type="checkbox" class="checkbox" name="{key}" value=""/>
    	    </td>
    			<td>
    				<xsl:value-of select="date"/>
    			</td>
    			<td>
            <xsl:if test="port = '0'">
        			<xsl:if test="not(name) or name = ''">
        			  ?
        			</xsl:if>
        			<xsl:if test="name and name != ''">
      	    		<xsl:value-of select="name"/>
        			</xsl:if>
            </xsl:if>
            <xsl:if test="port != '0'">
      			  <a href="javascript:void(0)" onClick="showMsgForm('{id}','{ip}','{port}','{name}','{pubkey}')">
        			  <xsl:if test="not(name) or name = ''">
        			    ?
        			  </xsl:if>
        			  <xsl:if test="name and name != ''">
      	    			<xsl:value-of select="name"/>
        			  </xsl:if>
              </a>
            </xsl:if>
    			</td>
    			<td>
     				<xsl:value-of select="msg"/>
    			</td>
  		  </xsl:if>
  		</tr>
    </xsl:if>
	</xsl:template>
</xsl:stylesheet>
