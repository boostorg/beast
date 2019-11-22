<!-- INCLUDES_FOOT_TEMPLATE BEGIN -->
<xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/core')]"     >core.hpp</xsl:template>
<xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/http')]"     >http.hpp</xsl:template>
<xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/ssl')]"      >ssl.hpp</xsl:template>
<xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/websocket')]">websocket.hpp</xsl:template>
<xsl:template mode="convenience-header" match="@file[contains(., 'boost/beast/zlib')]"     >zlib.hpp</xsl:template>
<xsl:template mode="convenience-header" match="@file"/>
<!-- INCLUDES_FOOT_TEMPLATE END -->
